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

#include <linux/vmalloc.h>

#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/util.h>

/**********************************************************************************************************************/

void *
direct_malloc( size_t bytes )
{
     void *mem;

     if (!bytes)
          return NULL;

     mem = vmalloc( bytes );
     if (!mem) {
          D_OOM();
          return NULL;
     }

     return mem;
}

void *
direct_calloc( size_t count, size_t bytes )
{
     void *mem;

     if (!count || !bytes)
          return NULL;

     mem = vmalloc( count * bytes );
     if (!mem) {
          D_OOM();
          return NULL;
     }

     memset( mem, 0, count * bytes );

     return mem;
}

void *
direct_realloc( void *oldmem, size_t bytes )
{
     void *mem;

     if (!bytes) {
          vfree( oldmem );
          return NULL;
     }

     mem = vmalloc( bytes );
     if (!mem) {
          D_OOM();
          return NULL;
     }

     direct_memcpy( mem, oldmem, bytes );    // FIXME: don't know how big buffer WAS! -> use preceding memory or hash

     return mem;
}

char *
direct_strdup( const char *str )
{
     void         *mem;
     unsigned int  length;

     if (!str)
          return NULL;

     length = strlen( str );

     mem = vmalloc( length + 1 );
     if (!mem) {
          D_OOM();
          return NULL;
     }

     direct_memcpy( mem, str, length + 1 );

     return mem;
}

void
direct_free( void *mem )
{
     if (mem)
          vfree( mem );
}

