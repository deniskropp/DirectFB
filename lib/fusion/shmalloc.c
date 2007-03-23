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

#include <fusion/fusion_internal.h>
#include <fusion/shm/shm_internal.h>


#if FUSION_BUILD_MULTI

/*************************** MULTI APPLICATION CORE ***************************/

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

D_DEBUG_DOMAIN( Fusion_SHM, "Fusion/SHM", "Fusion Shared Memory" );

void
fusion_dbg_print_memleaks( FusionSHMPoolShared *pool )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     unsigned int  total = 0;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return;
     }

     if (pool->allocs) {
          direct_log_printf( NULL, "\nShared memory allocations remaining (%d) in '%s': \n",
                             direct_list_count_elements_EXPENSIVE( pool->allocs ), pool->name );

          direct_list_foreach (desc, pool->allocs) {
               direct_log_printf( NULL, " %9d bytes at %p [%8lu] in %-30s [%3lx] (%s: %u)\n",
                                  desc->bytes, desc->mem, (ulong)desc->mem - (ulong)pool->heap,
                                  desc->func, desc->fid, desc->file, desc->line );

               total += desc->bytes;
          }

          direct_log_printf( NULL, "   -------\n  %7dk total\n", total >> 10 );
          direct_log_printf( NULL, "\nShared memory file size: %dk\n", pool->heap->size >> 10 );
     }

     fusion_skirmish_dismiss( &pool->lock );
}

static SHMemDesc *
fill_shmem_desc( SHMemDesc *desc, int bytes, const char *func, const char *file, int line, FusionID fusion_id )
{
     D_ASSERT( desc != NULL );

     desc->mem   = desc + 1;
     desc->bytes = bytes;

     snprintf( desc->func, SHMEMDESC_FUNC_NAME_LENGTH, func );
     snprintf( desc->file, SHMEMDESC_FILE_NAME_LENGTH, file );

     desc->line = line;
     desc->fid  = fusion_id;

     return desc;
}

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __size )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( __size > 0 );

     if (!pool->debug)
          return fusion_shmalloc( pool, __size );

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, __size + sizeof(SHMemDesc), false, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate %d bytes from pool!\n", __size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, __size, func, file, line, _fusion_id(pool->shm->world) );

     D_DEBUG_AT( Fusion_SHM, "allocating %9d bytes at %p [%8lu] in %-30s [%3lx] (%s: %u)\n",
                 desc->bytes, desc->mem, (ulong)desc->mem - (ulong)pool->heap,
                 desc->func, desc->fid, desc->file, desc->line );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_dbg_shcalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __nmemb, size_t __size)
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     if (!pool->debug)
          return fusion_shcalloc( pool, __nmemb, __size );

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, __nmemb * __size + sizeof(SHMemDesc), true, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate %d bytes from pool!\n", __nmemb * __size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, __nmemb * __size, func, file, line, _fusion_id(pool->shm->world) );

     D_DEBUG_AT( Fusion_SHM, "allocating %9d bytes at %p [%8lu] in %-30s [%3lx] (%s: %u)\n",
                 desc->bytes, desc->mem, (ulong)desc->mem - (ulong)pool->heap,
                 desc->func, desc->fid, desc->file, desc->line );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                      const char *file, int line,
                      const char *func, const char *what, void *__ptr,
                      size_t __size )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( what != NULL );

     if (!pool->debug)
          return fusion_shrealloc( pool, __ptr, __size );

     if (!__ptr)
          return fusion_dbg_shmalloc( pool, file, line, func, __size );

     if (!__size) {
          fusion_dbg_shfree( pool, file, line, func, what, __ptr );
          return NULL;
     }

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Lookup the corresponding description. */
     direct_list_foreach (desc, pool->allocs) {
          if (desc->mem == __ptr)
               break;
     }

     if (!desc) {
          D_ERROR( "Fusion/SHM: Cannot reallocate unknown chunk at %p (%s) from [%s:%d in %s()]!\n",
                   __ptr, what, file, line, func );
          D_BREAK( "unknown chunk" );
          return NULL; /* shouldn't happen due to the break */
     }

     /* Remove the description in case the block moves. */
     direct_list_remove( &pool->allocs, &desc->link );

     /* Reallocate the memory block. */
     ret = fusion_shm_pool_reallocate( pool, __ptr - sizeof(SHMemDesc), __size + sizeof(SHMemDesc), false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not reallocate from %d to %d bytes!\n",
                    desc->bytes + sizeof(SHMemDesc), __size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, __size, func, file, line, _fusion_id(pool->shm->world) );

     D_DEBUG_AT( Fusion_SHM, "reallocating %9d bytes at %p [%8lu] in %-30s [%3lx] (%s: %u) '%s'\n",
                 desc->bytes, desc->mem, (ulong)desc->mem - (ulong)pool->heap,
                 desc->func, desc->fid, desc->file, desc->line, what );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( FusionSHMPoolShared *pool,
                   const char *file, int line,
                   const char *func, const char *what, void *__ptr )
{
     DirectResult  ret;
     SHMemDesc    *desc;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( what != NULL );
     D_ASSERT( __ptr != NULL );

     if (!pool->debug)
          return fusion_shfree( pool, __ptr );

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return;
     }

     /* Lookup the corresponding description. */
     direct_list_foreach (desc, pool->allocs) {
          if (desc->mem == __ptr)
               break;
     }

     if (!desc) {
          D_ERROR( "Fusion/SHM: Cannot free unknown chunk at %p (%s) from [%s:%d in %s()]!\n",
                   __ptr, what, file, line, func );
          D_BREAK( "unknown chunk" );
          return; /* shouldn't happen due to the break */
     }

     D_DEBUG_AT( Fusion_SHM, "freeing %9d bytes at %p [%8lu] in %-30s [%3lx] (%s: %u) '%s'\n",
                 desc->bytes, desc->mem, (ulong)desc->mem - (ulong)pool->heap,
                 desc->func, desc->fid, desc->file, desc->line, what );

     /* Remove the description. */
     direct_list_remove( &pool->allocs, &desc->link );

     /* Free the memory block. */
     fusion_shm_pool_deallocate( pool, __ptr - sizeof(SHMemDesc), false );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, const char *string )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;
     int           length;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( string != NULL );

     if (!pool->debug)
          return fusion_shstrdup( pool, string );

     length = strlen( string ) + 1;

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, length + sizeof(SHMemDesc), false, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate %d bytes from pool!\n", length + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, length, func, file, line, _fusion_id(pool->shm->world) );

     D_DEBUG_AT( Fusion_SHM, "allocating %9d bytes at %p [%8lu] in %-30s [%3lx] (%s: %u) <- \"%s\"\n",
                 desc->bytes, desc->mem, (ulong)desc->mem - (ulong)pool->heap,
                 desc->func, desc->fid, desc->file, desc->line, string );

     D_DEBUG_AT( Fusion_SHM, "  -> allocs: %p\n", pool->allocs );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     /* Copy string content. */
     direct_memcpy( data + sizeof(SHMemDesc), string, length );

     return data + sizeof(SHMemDesc);
}

#else

void
fusion_dbg_print_memleaks( FusionSHMPoolShared *pool )
{
}

#endif

/**********************************************************************************************************************/

/* Allocate SIZE bytes of memory.  */
void *
fusion_shmalloc( FusionSHMPoolShared *pool, size_t __size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __size > 0 );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     if (fusion_shm_pool_allocate( pool, __size, false, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     return data;
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_shcalloc( FusionSHMPoolShared *pool, size_t __nmemb, size_t __size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     if (fusion_shm_pool_allocate( pool, __nmemb * __size, true, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     return data;
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_shrealloc( FusionSHMPoolShared *pool, void *__ptr, size_t __size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     if (!__ptr)
          return fusion_shmalloc( pool, __size );

     if (!__size) {
          fusion_shfree( pool, __ptr );
          return NULL;
     }

     if (fusion_shm_pool_reallocate( pool, __ptr, __size, true, &data ))
          return NULL;

     D_ASSERT( data != NULL || __size == 0 );

     return data;
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_shfree( FusionSHMPoolShared *pool, void *__ptr )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __ptr != NULL );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     fusion_shm_pool_deallocate( pool, __ptr, true );
}

/* Duplicate string in shared memory. */
char *
fusion_shstrdup( FusionSHMPoolShared *pool, const char* string )
{
     int   len;
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( string != NULL );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     len = strlen( string ) + 1;

     if (fusion_shm_pool_allocate( pool, len, false, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     direct_memcpy( data, string, len );

     return data;
}

#else

/************************** SINGLE APPLICATION CORE ***************************/

#include <direct/mem.h>

void
fusion_dbg_print_memleaks( FusionSHMPoolShared *pool )
{
}

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __size > 0 );

     if (pool->debug)
          return direct_malloc( file, line, func, __size );

     return malloc( __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_dbg_shcalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __nmemb, size_t __size)
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     if (pool->debug)
          return direct_calloc( file, line, func, __nmemb, __size );

     return calloc( __nmemb, __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                      const char *file, int line,
                      const char *func, const char *what, void *__ptr,
                      size_t __size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     if (pool->debug)
          return direct_realloc( file, line, func, what, __ptr, __size );

     return realloc( __ptr, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( FusionSHMPoolShared *pool,
                   const char *file, int line,
                   const char *func, const char *what, void *__ptr )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __ptr != NULL );

     if (pool->debug)
          direct_free( file, line, func, what, __ptr );
     else
          free( __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, const char *string )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( string != NULL );

     if (pool->debug)
          return direct_strdup( file, line, func, string );

     return strdup( string );
}

#endif

/**********************************************************************************************************************/

/* Allocate SIZE bytes of memory.  */
void *
fusion_shmalloc (FusionSHMPoolShared *pool,
                 size_t __size)
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __size > 0 );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     return malloc( __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_shcalloc (FusionSHMPoolShared *pool,
                 size_t __nmemb, size_t __size)
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     return calloc( __nmemb, __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_shrealloc (FusionSHMPoolShared *pool,
                  void *__ptr, size_t __size)
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     return realloc( __ptr, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_shfree (FusionSHMPoolShared *pool,
               void *__ptr)
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __ptr != NULL );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     free( __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_shstrdup (FusionSHMPoolShared *pool,
                 const char          *string)
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( string != NULL );

     if (pool->debug)
          D_WARN( "Fusion/SHMMalloc: Pool runs in debug mode, but access from pure-release is attempted!\n" );

     return strdup( string );
}

#endif

