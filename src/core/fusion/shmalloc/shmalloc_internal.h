/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __SHMALLOC_INTERNAL_H__
#define __SHMALLOC_INTERNAL_H__

#include <stddef.h>
#include <limits.h>

#include "../reactor.h"
#include "../lock.h"



/* Allocate SIZE bytes of memory.  */
void *_fusion_shmalloc (size_t __size);

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *_fusion_shrealloc (void *__ptr, size_t __size);

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *_fusion_shcalloc (size_t __nmemb, size_t __size);

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void  _fusion_shfree (void *__ptr);

/* Allocate SIZE bytes allocated to ALIGNMENT bytes.  */
void *_fusion_shmemalign (size_t __alignment, size_t __size);

/* Allocate SIZE bytes on a page boundary.  */
void *_fusion_shvalloc (size_t __size);

/* Pick up the current statistics. */
struct shmstats _fusion_shmstats (void);


void *__shmalloc_init (bool initialize);
void *__shmalloc_brk  (int increment);
void  __shmalloc_exit (bool shutdown);

ReactionResult __shmalloc_react (const void *msg_data, void *ctx);

/* Internal version of `shfree' used in `morecore' (shmalloc.c). */
void _fusion_shfree_internal (void *__ptr);


/* The allocator divides the heap into blocks of fixed size; large
   requests receive one or more whole blocks, and small requests
   receive a fragment of a block.  Fragment sizes are powers of two,
   and all fragments of a block are the same size.  When all the
   fragments in a block have been freed, the block itself is freed.  */
#define INT_BIT         (CHAR_BIT * sizeof(int))
#define BLOCKLOG        (INT_BIT > 16 ? 12 : 9)
#define BLOCKSIZE       (1 << BLOCKLOG)
#define BLOCKIFY(SIZE)  (((SIZE) + BLOCKSIZE - 1) / BLOCKSIZE)

/* Determine the amount of memory spanned by the initial heap table
   (not an absolute limit).  */
#define HEAP            (INT_BIT > 16 ? 4194304 : 65536)

/* Number of contiguous free blocks allowed to build up at the end of
   memory before they will be returned to the system.  */
#define FINAL_FREE_BLOCKS       8

/* Address to block number and vice versa.  */
#define BLOCK(A)        (((char *) (A) - _sheap->heapbase) / BLOCKSIZE + 1)
#define ADDRESS(B)      ((void *) (((B) - 1) * BLOCKSIZE + _sheap->heapbase))


/* Data structure giving per-block information.  */
typedef union {

     /* Heap information for a busy block.  */
     struct {

          /* Zero for a large block, or positive giving the
             logarithm to the base two of the fragment size.  */
          int type;

          union {
               struct {
                    size_t nfree;   /* Free fragments in a fragmented block.  */
                    size_t first;   /* First free fragment of the block.  */
               } frag;

               /* Size (in blocks) of a large cluster.  */
               size_t size;
          } info;
     } busy;

     /* Heap information for a free block
        (that may be the first of a free cluster).  */
     struct {
          size_t size;                /* Size (in blocks) of a free cluster.  */
          size_t next;                /* Index of next free cluster.  */
          size_t prev;                /* Index of previous free cluster.  */
     } free;
} shmalloc_info;

/* Doubly linked lists of free fragments.  */
struct list {
     struct list *next;
     struct list *prev;
};

/* List of blocks allocated with `shmemalign' (or `shvalloc').  */
struct alignlist {
     struct alignlist *next;
     void *aligned;                /* The address that shmemaligned returned.  */
     void *exact;          /* The address that shmalloc returned.  */
};

typedef struct {
     /* Pointer to first block of the heap.  */
     char *heapbase;

     /* Lock for heap management. */
     FusionSkirmish lock;

     /* Reactor for remapping messages. */
     FusionReactor *reactor;

     /* Block information table indexed by block number giving per-block information. */
     shmalloc_info *heapinfo;

     /* Number of info entries.  */
     size_t heapsize;

     /* Current search index for the heap table.  */
     size_t heapindex;

     /* Limit of valid info table indices.  */
     size_t heaplimit;

#if 1   /* Adapted from Mike */
     /* Count of large blocks allocated for each fragment size. */
     int fragblocks[BLOCKLOG];
#endif

     /* Free list headers for each fragment size.  */
     struct list fraghead[BLOCKLOG];

     /* List of blocks allocated by shmemalign.  */
     struct alignlist *aligned_blocks;

     /* Instrumentation.  */
     size_t chunks_used;
     size_t bytes_used;
     size_t chunks_free;
     size_t bytes_free;
} shmalloc_heap;

/* global data at beginning of shared memory */
extern shmalloc_heap *_sheap;

#endif /* shmalloc_internal.h  */
