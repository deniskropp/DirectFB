/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <direct/os/mem.h>

#include <direct/log.h>


#ifdef DIRECT_MEM_SENTINELS


#define PREFIX_SENTINEL (8)   // min = 8!    (for size_t of memory block)
#define SUFFIX_SENTINEL (8)


#define TOTAL_SENTINEL  ((PREFIX_SENTINEL) + (SUFFIX_SENTINEL))


__attribute__((no_instrument_function))
static inline void
install_sentinels( void *p, size_t size )
{
     size_t  i;
     size_t *ps     = p;
     u8     *prefix = p;
     u8     *suffix = p + PREFIX_SENTINEL + size;

//     direct_log_printf( NULL, "Direct/Mem: Installing sentinels at %p (%zu bytes allocation)...\n", p, size );

     *ps = size;

     for (i=sizeof(size_t); i<PREFIX_SENTINEL; i++)
          prefix[i] = i;

     for (i=0; i<SUFFIX_SENTINEL; i++)
          suffix[i] = i;
}

__attribute__((no_instrument_function))
static inline void
remove_sentinels( void *p )
{
     size_t  i;
     size_t *ps     = p;
     u8     *prefix = p;
     u8     *suffix = p + PREFIX_SENTINEL + *ps;

     for (i=sizeof(size_t); i<PREFIX_SENTINEL; i++)
          prefix[i] = 0;

     for (i=0; i<SUFFIX_SENTINEL; i++)
          suffix[i] = 0;
}

__attribute__((no_instrument_function))
static inline void
check_sentinels( void *p )
{
     size_t  i;
     size_t *ps     = p;
     u8     *prefix = p;
     u8     *suffix = p + PREFIX_SENTINEL + *ps;

     for (i=sizeof(size_t); i<PREFIX_SENTINEL; i++) {
          if (prefix[i] != (i & 0xff))
               direct_log_printf( NULL, "Direct/Mem: Sentinel error at prefix[%zu] (%u) of %zu bytes allocation!\n",
                                  i & 0xff, prefix[i], *ps );
     }

     for (i=0; i<SUFFIX_SENTINEL; i++) {
          if (suffix[i] != (i & 0xff))
               direct_log_printf( NULL, "Direct/Mem: Sentinel error at suffix[%zu] (%u) of %zu bytes allocation!\n",
                                  i & 0xff, suffix[i], *ps );
     }
}


__attribute__((no_instrument_function))
void *
direct_malloc( size_t bytes )
{
     void *p = bytes ? malloc( bytes + TOTAL_SENTINEL ) : NULL;

     if (!p)
          return NULL;

     install_sentinels( p, bytes );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
void *
direct_calloc( size_t count, size_t bytes)
{
     void *p = (count && bytes) ? calloc( 1, count * bytes + TOTAL_SENTINEL ) : NULL;

     if (!p)
          return NULL;

     install_sentinels( p, count * bytes );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
void *
direct_realloc( void *mem, size_t bytes )
{
     void *p = mem ? mem - PREFIX_SENTINEL : NULL;

     if (!mem)
          return direct_malloc( bytes );
          
     check_sentinels( p );

     if (!bytes) {
          direct_free( mem );
          return NULL;
     }

     p = realloc( p, bytes + TOTAL_SENTINEL );

     if (!p)
          return NULL;

     install_sentinels( p, bytes );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
char *
direct_strdup( const char *str )
{
     int   n = strlen( str );
     void *p = malloc( n+1 + TOTAL_SENTINEL );

     if (!p)
          return NULL;

     memcpy( p + PREFIX_SENTINEL, str, n+1 );

     install_sentinels( p, n+1 );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
void
direct_free( void *mem )
{
     void *p = mem ? mem - PREFIX_SENTINEL : NULL;

     if (p) {
          check_sentinels( p );
     
          remove_sentinels( p );
     
          free( p );
     }
}



#else

__attribute__((no_instrument_function))
void *
direct_malloc( size_t bytes )
{
     return malloc( bytes );
}

__attribute__((no_instrument_function))
void *
direct_calloc( size_t count, size_t bytes)
{
     return calloc( count, bytes );
}

__attribute__((no_instrument_function))
void *
direct_realloc( void *mem, size_t bytes )
{
     return realloc( mem, bytes );
}

__attribute__((no_instrument_function))
char *
direct_strdup( const char *str )
{
     return strdup( str );
}

__attribute__((no_instrument_function))
void
direct_free( void *mem )
{
     free( mem );
}

#endif

