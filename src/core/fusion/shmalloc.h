/*
   (c) Copyright 2001  Denis Oliver Kropp <dok@directfb.org>
   All rights reserved.

   Fusion shmalloc is based on GNU malloc. Please see below.

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

/* Declarations for `malloc' and friends.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
                  Written May 1989 by Mike Haertel.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation.  */

#ifndef __SHMALLOC_H__
#define __SHMALLOC_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef FUSION_FAKE

#include <stddef.h>

/* Allocate SIZE bytes of memory.  */
void *shmalloc (size_t __size);

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *shrealloc (void *__ptr, size_t __size);

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *shcalloc (size_t __nmemb, size_t __size);

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void  shfree (void *__ptr);

/* Allocate SIZE bytes allocated to ALIGNMENT bytes.  */
void *shmemalign (size_t __alignment, size_t __size);

/* Allocate SIZE bytes on a page boundary.  */
void *shvalloc (size_t __size);


/* Statistics available to the user.  */
struct shmstats {
    size_t bytes_total;         /* Total size of the heap. */
    size_t chunks_used;         /* Chunks allocated by the user. */
    size_t bytes_used;          /* Byte total of user-allocated chunks. */
    size_t chunks_free;         /* Chunks in the free list. */
    size_t bytes_free;          /* Byte total of chunks in the free list. */
};

/* Pick up the current statistics. */
struct shmstats shmstats (void);

#else

     #define shmalloc  malloc
     #define shrealloc realloc
     #define shcalloc  calloc
     #define shfree    free

#endif

#ifdef __cplusplus
}
#endif

#endif /* __SHMALLOC_H__ */

