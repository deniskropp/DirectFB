/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __FUSION__SHM__SHM_INTERNAL_H__
#define __FUSION__SHM__SHM_INTERNAL_H__

#include <limits.h>

#include <direct/list.h>

#include <fusion/build.h>
#include <fusion/lock.h>


#define FUSION_SHM_MAX_POOLS                 16
#define FUSION_SHM_TMPFS_PATH_NAME_LEN       64


typedef struct __shmalloc_heap shmalloc_heap;


/*
 * Local pool data.
 */
struct __Fusion_FusionSHMPool {
     int                  magic;

     bool                 attached;     /* Indicates usage of this entry in the static pool array. */

     FusionSHM           *shm;          /* Back pointer to local SHM data. */

     FusionSHMPoolShared *shared;       /* Pointer to shared pool data. */

     int                  pool_id;      /* The pool's ID within the world. */

     char                *filename;     /* Name of the shared memory file. */
};

/*
 * Shared pool data.
 */
struct __Fusion_FusionSHMPoolShared {
     int                  magic;

     bool                 debug;        /* Debug allocations in this pool? */

     int                  index;        /* Index within the static pool array. */
     bool                 active;       /* Indicates usage of this entry in the static pool array. */

     FusionSHMShared     *shm;          /* Back pointer to shared SHM data. */

     int                  max_size;     /* Maximum possible size of the shared memory. */
     int                  pool_id;      /* The pool's ID within the world. */
     void                *addr_base;    /* Virtual starting address of shared memory. */

     FusionSkirmish       lock;         /* Lock for this pool. */

     shmalloc_heap       *heap;         /* The actual heap information ported from libc5. */

     char                *name;         /* Name of the pool (allocated in the pool). */

     DirectLink          *allocs;       /* Used for debugging. */
};


/*
 * Local SHM data.
 */
struct __Fusion_FusionSHM {
     int                  magic;

     FusionWorld         *world;        /* Back pointer to local world data. */

     FusionSHMShared     *shared;       /* Pointer to shared SHM data. */

     FusionSHMPool        pools[FUSION_SHM_MAX_POOLS]; /* Local data of all pools. */

     DirectSignalHandler *signal_handler;
};

/*
 * Shared SHM data.
 */
struct __Fusion_FusionSHMShared {
     int                  magic;

     FusionWorldShared   *world;        /* Back pointer to shared world data. */

     FusionSkirmish       lock;         /* Lock for list of pools. */

     int                  num_pools;    /* Number of active pools. */
     FusionSHMPoolShared  pools[FUSION_SHM_MAX_POOLS]; /* Shared data of all pools. */

     char                 tmpfs[FUSION_SHM_TMPFS_PATH_NAME_LEN];
};



/* The allocator divides the heap into blocks of fixed size; large
   requests receive one or more whole blocks, and small requests
   receive a fragment of a block.  Fragment sizes are powers of two,
   and all fragments of a block are the same size.  When all the
   fragments in a block have been freed, the block itself is freed.  */
#define INT_BIT          (CHAR_BIT * sizeof(int))
#define BLOCKLOG         (INT_BIT > 16 ? 12 : 9)
#define BLOCKSIZE        (1 << BLOCKLOG)
#define BLOCKIFY(SIZE)   (((SIZE) + BLOCKSIZE - 1) / BLOCKSIZE)
#define BLOCKALIGN(SIZE) (((SIZE) + BLOCKSIZE - 1) & ~(BLOCKSIZE - 1))

/* Number of contiguous free blocks allowed to build up at the end of
   memory before they will be returned to the system.  */
#define FINAL_FREE_BLOCKS       8

/* Address to block number and vice versa.  */
#define BLOCK(A)        (((char *) (A) - heap->heapbase) / BLOCKSIZE + 1)
#define ADDRESS(B)      ((void *) (((B) - 1) * BLOCKSIZE + heap->heapbase))


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


#define SHMEMDESC_FUNC_NAME_LENGTH 48
#define SHMEMDESC_FILE_NAME_LENGTH 24

/* Used for debugging. */
typedef struct {
     DirectLink    link;

     const void   *mem;
     size_t        bytes;
     char          func[SHMEMDESC_FUNC_NAME_LENGTH];
     char          file[SHMEMDESC_FILE_NAME_LENGTH];
     unsigned int  line;

     FusionID      fid;
} SHMemDesc;


struct __shmalloc_heap {
     int magic;

     /* Pointer to first block of the heap.  */
     char *heapbase;

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

     /* Instrumentation.  */
     size_t chunks_used;
     size_t bytes_used;
     size_t chunks_free;
     size_t bytes_free;

     /* Total size of heap in bytes. */
     int size;

     /* Back pointer to shared memory pool. */
     FusionSHMPoolShared *pool;

     char filename[FUSION_SHM_TMPFS_PATH_NAME_LEN+32];
};


void *_fusion_shmalloc (shmalloc_heap *heap, size_t __size);

void *_fusion_shrealloc (shmalloc_heap *heap, void *__ptr, size_t __size);

void  _fusion_shfree (shmalloc_heap *heap, void *__ptr);


DirectResult __shmalloc_init_heap( FusionSHM     *shm,
                                   const char    *filename,
                                   void          *addr_base,
                                   int            space,
                                   int           *ret_size );

DirectResult __shmalloc_join_heap( FusionSHM     *shm,
                                   const char    *filename,
                                   void          *addr_base,
                                   int            size,
                                   bool           write );

void        *__shmalloc_brk      ( shmalloc_heap *heap,
                                   int            increment );


#endif

