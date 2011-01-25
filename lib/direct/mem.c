/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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


#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>


#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */


#define DISABLED_OFFSET (8)


D_LOG_DOMAIN( Direct_Mem, "Direct/Mem", "libdirect memory allocation (debugging)" );

/**********************************************************************************************************************/

typedef struct {
     void              *mem;
     size_t             bytes;
     const char        *func;
     const char        *file;
     int                line;
     DirectTraceBuffer *trace;
} MemDesc;

/**********************************************************************************************************************/

static DirectHash  alloc_hash = DIRECT_HASH_INIT( 523, true );
static DirectMutex alloc_lock;

void
__D_mem_init()
{
     direct_mutex_init( &alloc_lock );
}

void
__D_mem_deinit()
{
     direct_mutex_deinit( &alloc_lock );
}

/**********************************************************************************************************************/

static bool
local_alloc_hash_iterator( DirectHash    *hash,
                           unsigned long  key,
                           void          *value,
                           void          *ctx )
{
     MemDesc       *desc  = value;
     unsigned long *total = ctx;

     direct_log_printf( NULL, "%7zu bytes at %p allocated in %s (%s: %u)\n",
                        desc->bytes, desc->mem, desc->func, desc->file, desc->line );

     if (desc->trace)
          direct_trace_print_stack( desc->trace );

     *total += desc->bytes;

     return true;
}

void
direct_print_memleaks( void )
{
     unsigned long total = 0;

     /* Debug only. */
     direct_mutex_lock( &alloc_lock );

     if (alloc_hash.count) {
          direct_log_printf( NULL, "Local memory allocations remaining (%d): \n", alloc_hash.count );

          direct_hash_iterate( &alloc_hash, local_alloc_hash_iterator, &total );
     }

     direct_mutex_unlock( &alloc_lock );

     if (total)
          direct_log_printf( NULL, "%7lu bytes in total\n", total );
}

/**********************************************************************************************************************/

__no_instrument_function__
static __inline__ MemDesc *
fill_mem_desc( MemDesc *desc, int bytes, const char *func, const char *file, int line, DirectTraceBuffer *trace )
{
     D_ASSERT( desc != NULL );

     desc->mem   = desc + 1;
     desc->bytes = bytes;
     desc->func  = func;
     desc->file  = file;
     desc->line  = line;
     desc->trace = trace;

     return desc;
}

/**********************************************************************************************************************/

void *
direct_dbg_malloc( const char* file, int line, const char *func, size_t bytes )
{
     void          *mem;
     unsigned long *val;

     D_DEBUG_AT( Direct_Mem, "  +%6zu bytes [%s:%d in %s()]\n", bytes, file, line, func );

     if (direct_config->debugmem) {
          MemDesc *desc;

          mem = direct_malloc( bytes + sizeof(MemDesc) );

          D_DEBUG_AT( Direct_Mem, "  '-> %p\n", mem );

          if (!mem)
               return NULL;

          desc = fill_mem_desc( mem, bytes, func, file, line, direct_trace_copy_buffer(NULL) );

          direct_mutex_lock( &alloc_lock );

          direct_hash_insert( &alloc_hash, (unsigned long) desc->mem, desc );

          direct_mutex_unlock( &alloc_lock );

          return desc->mem;
     }


     mem = direct_malloc( bytes + DISABLED_OFFSET );
     if (!mem)
          return NULL;

     D_DEBUG_AT( Direct_Mem, "  '-> %p\n", mem );

     val    = mem;
     val[0] = ~0;

     return (char*) mem + DISABLED_OFFSET;
}

void *
direct_dbg_calloc( const char* file, int line, const char *func, size_t count, size_t bytes )
{
     void          *mem;
     unsigned long *val;

     D_DEBUG_AT( Direct_Mem, "  +%6zu bytes [%s:%d in %s()]\n", count * bytes, file, line, func );

     if (direct_config->debugmem) {
          MemDesc *desc;

          mem = direct_calloc( 1, count * bytes + sizeof(MemDesc) );

          D_DEBUG_AT( Direct_Mem, "  '-> %p\n", mem );

          if (!mem)
               return NULL;

          desc = fill_mem_desc( mem, count * bytes, func, file, line, direct_trace_copy_buffer(NULL) );

          direct_mutex_lock( &alloc_lock );

          direct_hash_insert( &alloc_hash, (unsigned long) desc->mem, desc );

          direct_mutex_unlock( &alloc_lock );

          return desc->mem;
     }


     mem = direct_calloc( 1, count * bytes + DISABLED_OFFSET );

     D_DEBUG_AT( Direct_Mem, "  '-> %p\n", mem );

     if (!mem)
          return NULL;

     val    = mem;
     val[0] = ~0;

     return (char*) mem + DISABLED_OFFSET;
}

void *
direct_dbg_realloc( const char *file, int line, const char *func, const char *what, void *mem, size_t bytes )
{
     unsigned long *val;
     MemDesc       *desc;

     D_DEBUG_AT( Direct_Mem, "  *%6zu bytes [%s:%d in %s()] '%s' <- %p\n", bytes, file, line, func, what, mem );

     if (!mem)
          return direct_dbg_malloc( file, line, func, bytes );

     if (!bytes) {
          direct_dbg_free( file, line, func, what, mem );
          return NULL;
     }

     val = (unsigned long*)((char*) mem - DISABLED_OFFSET);

     if (val[0] == ~0) {
          D_DEBUG_AT( Direct_Mem, "  *%6zu bytes [%s:%d in %s()] '%s'\n", bytes, file, line, func, what );

          val = direct_realloc( val, bytes + DISABLED_OFFSET );

          D_DEBUG_AT( Direct_Mem, "  '-> %p\n", val );

          if (val) {
               D_ASSERT( val[0] == ~0 );

               mem = val;

               return (char*) mem + DISABLED_OFFSET;
          }

          return NULL;
     }

     /* Debug only. */
     direct_mutex_lock( &alloc_lock );

     desc = (MemDesc*)((char*) mem - sizeof(MemDesc));
     D_ASSERT( desc->mem == mem );

     if (direct_hash_remove( &alloc_hash, (unsigned long) mem )) {
          D_ERROR( "Direct/Mem: Not reallocating unknown %p (%s) from [%s:%d in %s()] - corrupt/incomplete list?\n",
                   mem, what, file, line, func );

          mem = direct_dbg_malloc( file, line, func, bytes );
     }
     else {
          void *new_mem = direct_realloc( desc, bytes + sizeof(MemDesc) );

          D_DEBUG_AT( Direct_Mem, "  '-> %p\n", new_mem );

          D_DEBUG_AT( Direct_Mem, "  %c%6zu bytes [%s:%d in %s()] (%s%zu) <- %p -> %p '%s'\n",
                      (bytes > desc->bytes) ? '>' : '<', bytes, file, line, func,
                      (bytes > desc->bytes) ? "+" : "", bytes - desc->bytes, mem, new_mem, what);

          if (desc->trace) {
               direct_trace_free_buffer( desc->trace );
               desc->trace = NULL;
          }

          if (!new_mem)
               D_WARN( "could not reallocate memory (%p: %zu->%zu)", mem, desc->bytes, bytes );
          else
               desc = fill_mem_desc( new_mem, bytes, func, file, line, direct_trace_copy_buffer(NULL) );


          mem = desc->mem;
     }

     direct_mutex_unlock( &alloc_lock );

     return mem;
}

char *
direct_dbg_strdup( const char* file, int line, const char *func, const char *string )
{
     void          *mem;
     unsigned long *val;
     size_t         bytes = direct_strlen( string ) + 1;

     D_DEBUG_AT( Direct_Mem, "  +%6zu bytes [%s:%d in %s()] <- \"%30s\"\n", bytes, file, line, func, string );

     if (direct_config->debugmem) {
          MemDesc *desc;

          mem = direct_malloc( bytes + sizeof(MemDesc) );

          D_DEBUG_AT( Direct_Mem, "  '-> %p\n", mem );

          if (!mem)
               return NULL;

          desc = fill_mem_desc( mem, bytes, func, file, line, direct_trace_copy_buffer(NULL) );

          direct_mutex_lock( &alloc_lock );

          direct_hash_insert( &alloc_hash, (unsigned long) desc->mem, desc );

          direct_mutex_unlock( &alloc_lock );

          direct_memcpy( desc->mem, string, bytes );

          return desc->mem;
     }


     mem = direct_malloc( bytes + DISABLED_OFFSET );

     D_DEBUG_AT( Direct_Mem, "  '-> %p\n", mem );

     if (!mem)
          return NULL;

     val    = mem;
     val[0] = ~0;

     direct_memcpy( (char*) mem + DISABLED_OFFSET, string, bytes );

     return (char*) mem + DISABLED_OFFSET;
}

void
direct_dbg_free( const char *file, int line, const char *func, const char *what, void *mem )
{
     unsigned long *val;
     MemDesc       *desc;

     if (!mem) {
          D_WARN( "%s (NULL) called", __FUNCTION__ );
          return;
     }

     val = (unsigned long*)((char*) mem - DISABLED_OFFSET);

     if (val[0] == ~0) {
          D_DEBUG_AT( Direct_Mem, "  - number of bytes of %s [%s:%d in %s()] -> %p\n", what, file, line, func, mem );

          val[0] = 0;
          direct_free( val );

          return;
     }


     /* Debug only. */
     direct_mutex_lock( &alloc_lock );

     desc = (MemDesc*)((char*) mem - sizeof(MemDesc));
     D_ASSERT( desc->mem == mem );

     if (direct_hash_remove( &alloc_hash, (unsigned long) mem )) {
          D_ERROR( "Direct/Mem: Not freeing unknown %p (%s) from [%s:%d in %s()] - corrupt/incomplete list?\n",
                   mem, what, file, line, func );
     }
     else {
          D_DEBUG_AT( Direct_Mem, "  -%6zu bytes [%s:%d in %s()] -> %p '%s'\n", desc->bytes, file, line, func, mem, what );

          if (desc->trace)
               direct_trace_free_buffer( desc->trace );

          direct_free( desc );
     }

     direct_mutex_unlock( &alloc_lock );
}

/**********************************************************************************************************************/

#else

void
__D_mem_init()
{
}

void
__D_mem_deinit()
{
}

void
direct_print_memleaks( void )
{
}

#endif

