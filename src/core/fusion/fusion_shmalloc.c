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
#include <time.h>

#include <core/coredefs.h>

#include <misc/mem.h>
#include <misc/memcpy.h>

#include "shmalloc.h"


#ifndef FUSION_FAKE

#include "shmalloc/shmalloc_internal.h"

/* Allocate SIZE bytes of memory.  */
void *
shmalloc (size_t __size)
{
     void *ret;

     DFB_ASSERT( __size > 0 );
     DFB_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shmalloc( __size );
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
shrealloc (void *__ptr, size_t __size)
{
     void *ret;

     DFB_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shrealloc( __ptr, __size );
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
shcalloc (size_t __nmemb, size_t __size)
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

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
shfree (void *__ptr)
{
     DFB_ASSERT( __ptr != NULL );
     DFB_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );
     
     _fusion_shfree( __ptr );
     
     fusion_skirmish_dismiss( &_sheap->lock );
}

/* Duplicate string in shared memory. */
char *
shstrdup (const char* string)
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

#else

/* Allocate SIZE bytes of memory.  */
void *
shmalloc (size_t __size)
{
     DFB_ASSERT( __size > 0 );

     return DFBMALLOC( __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
shrealloc (void *__ptr, size_t __size)
{
     return DFBREALLOC( __ptr, __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
shcalloc (size_t __nmemb, size_t __size)
{
     DFB_ASSERT( __nmemb > 0 );
     DFB_ASSERT( __size > 0 );

     return DFBCALLOC( __nmemb, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
shfree (void *__ptr)
{
     DFB_ASSERT( __ptr != NULL );

     DFBFREE( __ptr );
}

/* Duplicate string in shared memory. */
char *
shstrdup (const char* string)
{
     DFB_ASSERT( string != NULL );

     return DFBSTRDUP( string );
}

#endif

