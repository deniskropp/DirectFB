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

/* Heap management adapted from libc
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

#include <config.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/shmalloc.h>
#include <fusion/fusion_internal.h>

#include <fusion/shm/pool.h>
#include <fusion/shm/shm_internal.h>


D_DEBUG_DOMAIN( Fusion_SHMHeap, "Fusion/SHMHeap", "Fusion Shared Memory Heap" );

/**********************************************************************************************************************/

/* Aligned allocation.  */
static void *
align( shmalloc_heap *heap, size_t size )
{
     void          *result;
     unsigned long  adj;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %d )\n", __FUNCTION__, heap, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     result = __shmalloc_brk( heap, size );

     adj = (unsigned long) result % BLOCKSIZE;
     if (adj != 0) {
          adj = BLOCKSIZE - adj;
          __shmalloc_brk( heap, adj );
          result = (char *) result + adj;
     }

     return result;
}

/* Get neatly aligned memory, initializing or
   growing the heap info table as necessary. */
static void *
morecore( shmalloc_heap *heap, size_t size )
{
     void *result;
     shmalloc_info *newinfo, *oldinfo;
     size_t newsize;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %d )\n", __FUNCTION__, heap, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     result = align( heap, size );
     if (result == NULL)
          return NULL;

     /* Check if we need to grow the info table.  */
     if ((size_t) BLOCK ((char *) result + size) > heap->heapsize) {
          newsize = heap->heapsize;

          while ((size_t) BLOCK ((char *) result + size) > newsize)
               newsize *= 2;

          newinfo = (shmalloc_info *) align( heap, newsize * sizeof (shmalloc_info) );
          if (newinfo == NULL) {
               __shmalloc_brk( heap, -size );
               return NULL;
          }

          direct_memcpy( newinfo, heap->heapinfo,
                         heap->heapsize * sizeof (shmalloc_info) );

          memset (newinfo + heap->heapsize,
                  0, (newsize - heap->heapsize) * sizeof (shmalloc_info));

          oldinfo = heap->heapinfo;

          newinfo[BLOCK (oldinfo)].busy.type = 0;
          newinfo[BLOCK (oldinfo)].busy.info.size = BLOCKIFY (heap->heapsize * sizeof (shmalloc_info));

          heap->heapinfo = newinfo;

          _fusion_shfree( heap, oldinfo );

          heap->heapsize = newsize;
     }

     heap->heaplimit = BLOCK ((char *) result + size);

     return result;
}

/**********************************************************************************************************************/

/* Allocate memory from the heap.  */
void *
_fusion_shmalloc( shmalloc_heap *heap, size_t size )
{
     void *result;
     size_t block, blocks, lastblocks, start;
     register size_t i;
     struct list *next;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %d )\n", __FUNCTION__, heap, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     /* Some programs will call shmalloc (0). We let them pass. */
     if (size == 0)
          return NULL;

     if (size < sizeof (struct list))
          size = sizeof (struct list);

     /* Determine the allocation policy based on the request size.  */
     if (size <= BLOCKSIZE / 2) {
          /* Small allocation to receive a fragment of a block.
             Determine the logarithm to base two of the fragment size. */
          register size_t log = 1;
          --size;
          while ((size /= 2) != 0)
               ++log;

          /* Look in the fragment lists for a
             free fragment of the desired size. */
          next = heap->fraghead[log].next;
          if (next != NULL) {
               /* There are free fragments of this size.
                  Pop a fragment out of the fragment list and return it.
                  Update the block's nfree and first counters. */
               result = (void *) next;
               next->prev->next = next->next;
               if (next->next != NULL)
                    next->next->prev = next->prev;
               block = BLOCK (result);
               if (--(heap->heapinfo[block].busy.info.frag.nfree) != 0)
                    heap->heapinfo[block].busy.info.frag.first = (unsigned long int)
                                                                   ((unsigned long int) ((char *) next->next - (char *) NULL)
                                                                    % BLOCKSIZE) >> log;

               /* Update the statistics.  */
               heap->chunks_used++;
               heap->bytes_used += 1 << log;
               heap->chunks_free--;
               heap->bytes_free -= 1 << log;
          }
          else {
               /* No free fragments of the desired size, so get a new block
                  and break it into fragments, returning the first.  */
               result = _fusion_shmalloc( heap, BLOCKSIZE );
               if (result == NULL)
                    return NULL;
#if 1   /* Adapted from Mike */
               heap->fragblocks[log]++;
#endif

               /* Link all fragments but the first into the free list.  */
               for (i = 1; i < (size_t) (BLOCKSIZE >> log); ++i) {
                    next = (struct list *) ((char *) result + (i << log));
                    next->next = heap->fraghead[log].next;
                    next->prev = &heap->fraghead[log];
                    next->prev->next = next;
                    if (next->next != NULL)
                         next->next->prev = next;
               }

               /* Initialize the nfree and first counters for this block.  */
               block = BLOCK (result);
               heap->heapinfo[block].busy.type = log;
               heap->heapinfo[block].busy.info.frag.nfree = i - 1;
               heap->heapinfo[block].busy.info.frag.first = i - 1;

               heap->chunks_free += (BLOCKSIZE >> log) - 1;
               heap->bytes_free += BLOCKSIZE - (1 << log);
               heap->bytes_used -= BLOCKSIZE - (1 << log);
          }
     }
     else {
          /* Large allocation to receive one or more blocks.
             Search the free list in a circle starting at the last place visited.
             If we loop completely around without finding a large enough
             space we will have to get more memory from the system.  */
          blocks = BLOCKIFY (size);
          start = block = heap->heapindex;
          while (heap->heapinfo[block].free.size < blocks) {
               block = heap->heapinfo[block].free.next;
               if (block == start) {
                    /* Need to get more from the system.  Check to see if
                       the new core will be contiguous with the final free
                       block; if so we don't need to get as much.  */
                    block = heap->heapinfo[0].free.prev;
                    lastblocks = heap->heapinfo[block].free.size;
                    if (heap->heaplimit != 0 && block + lastblocks == heap->heaplimit &&
                        __shmalloc_brk( heap, 0 ) == ADDRESS (block + lastblocks) &&
                        (morecore( heap, (blocks - lastblocks) * BLOCKSIZE) ) != NULL) {
#if 1   /* Adapted from Mike */

                         /* Note that morecore() can change the location of
                            the final block if it moves the info table and the
                            old one gets coalesced into the final block. */
                         block = heap->heapinfo[0].free.prev;
                         heap->heapinfo[block].free.size += blocks - lastblocks;
#else
                         heap->heapinfo[block].free.size = blocks;
#endif
                         heap->bytes_free += (blocks - lastblocks) * BLOCKSIZE;
                         continue;
                    }
                    result = morecore( heap, blocks * BLOCKSIZE );
                    if (result == NULL)
                         return NULL;
                    block = BLOCK (result);
                    heap->heapinfo[block].busy.type = 0;
                    heap->heapinfo[block].busy.info.size = blocks;
                    heap->chunks_used++;
                    heap->bytes_used += blocks * BLOCKSIZE;
                    return result;
               }
          }

          /* At this point we have found a suitable free list entry.
             Figure out how to remove what we need from the list. */
          result = ADDRESS (block);
          if (heap->heapinfo[block].free.size > blocks) {
               /* The block we found has a bit left over,
                  so relink the tail end back into the free list. */
               heap->heapinfo[block + blocks].free.size
               = heap->heapinfo[block].free.size - blocks;
               heap->heapinfo[block + blocks].free.next
               = heap->heapinfo[block].free.next;
               heap->heapinfo[block + blocks].free.prev
               = heap->heapinfo[block].free.prev;
               heap->heapinfo[heap->heapinfo[block].free.prev].free.next
               = heap->heapinfo[heap->heapinfo[block].free.next].free.prev
                 = heap->heapindex = block + blocks;
          }
          else {
               /* The block exactly matches our requirements,
                  so just remove it from the list. */
               heap->heapinfo[heap->heapinfo[block].free.next].free.prev
               = heap->heapinfo[block].free.prev;
               heap->heapinfo[heap->heapinfo[block].free.prev].free.next
               = heap->heapindex = heap->heapinfo[block].free.next;
               heap->chunks_free--;
          }

          heap->heapinfo[block].busy.type = 0;
          heap->heapinfo[block].busy.info.size = blocks;
          heap->chunks_used++;
          heap->bytes_used += blocks * BLOCKSIZE;
          heap->bytes_free -= blocks * BLOCKSIZE;
     }

     return result;
}

/* Resize the given region to the new size, returning a pointer
   to the (possibly moved) region.  This is optimized for speed;
   some benchmarks seem to indicate that greater compactness is
   achieved by unconditionally allocating and copying to a
   new region.  This module has incestuous knowledge of the
   internals of both free and shmalloc. */
void *
_fusion_shrealloc( shmalloc_heap *heap, void *ptr, size_t size )
{
     void  *result;
     int    type;
     size_t block, blocks, oldlimit;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %p, %d )\n", __FUNCTION__, heap, ptr, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     if (ptr == NULL)
          return _fusion_shmalloc( heap, size );
     else if (size == 0) {
          _fusion_shfree( heap, ptr );
          return NULL;
     }

     block = BLOCK (ptr);

     type = heap->heapinfo[block].busy.type;
     switch (type) {
          case 0:
               /* Maybe reallocate a large block to a small fragment.  */
               if (size <= BLOCKSIZE / 2) {
                    result = _fusion_shmalloc( heap, size );
                    if (result != NULL) {
                         direct_memcpy (result, ptr, size);
                         _fusion_shfree( heap, ptr );
                         return result;
                    }
               }

               /* The new size is a large allocation as well;
                  see if we can hold it in place. */
               blocks = BLOCKIFY (size);
               if (blocks < heap->heapinfo[block].busy.info.size) {
                    /* The new size is smaller; return
                       excess memory to the free list. */
                    heap->heapinfo[block + blocks].busy.type = 0;
                    heap->heapinfo[block + blocks].busy.info.size
                    = heap->heapinfo[block].busy.info.size - blocks;
                    heap->heapinfo[block].busy.info.size = blocks;
                    _fusion_shfree( heap, ADDRESS (block + blocks) );
                    result = ptr;
               }
               else if (blocks == heap->heapinfo[block].busy.info.size)
                    /* No size change necessary.  */
                    result = ptr;
               else {
                    /* Won't fit, so allocate a new region that will.
                       Free the old region first in case there is sufficient
                       adjacent free space to grow without moving. */
                    blocks = heap->heapinfo[block].busy.info.size;
                    /* Prevent free from actually returning memory to the system.  */
                    oldlimit = heap->heaplimit;
                    heap->heaplimit = 0;
                    _fusion_shfree( heap, ptr );
                    heap->heaplimit = oldlimit;
                    result = _fusion_shmalloc( heap, size );
                    if (result == NULL) {
                         /* Now we're really in trouble.  We have to unfree
                            the thing we just freed.  Unfortunately it might
                            have been coalesced with its neighbors.  */
                         if (heap->heapindex == block)
                              (void) _fusion_shmalloc( heap, blocks * BLOCKSIZE );
                         else {
                              void *previous = _fusion_shmalloc( heap, (block - heap->heapindex) * BLOCKSIZE );
                              (void) _fusion_shmalloc( heap, blocks * BLOCKSIZE );
                              _fusion_shfree( heap, previous );
                         }
                         return NULL;
                    }
                    if (ptr != result)
                         direct_memmove (result, ptr, blocks * BLOCKSIZE);
               }
               break;

          default:
               /* Old size is a fragment; type is logarithm
                  to base two of the fragment size.  */
               if (size > (size_t) (1 << (type - 1)) && size <= (size_t) (1 << type))
                    /* The new size is the same kind of fragment.  */
                    result = ptr;
               else {
                    /* The new size is different; allocate a new space,
                       and copy the lesser of the new size and the old. */
                    result = _fusion_shmalloc( heap, size );
                    if (result == NULL)
                         return NULL;
                    direct_memcpy (result, ptr, MIN (size, (size_t) 1 << type));
                    _fusion_shfree( heap, ptr );
               }
               break;
     }

     return result;
}

/* Return memory to the heap. */
void
_fusion_shfree( shmalloc_heap *heap, void *ptr )
{
     int type;
     size_t block, blocks;
     register size_t i;
     struct list *prev, *next;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %p )\n", __FUNCTION__, heap, ptr );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     if (ptr == NULL)
          return;

     block = BLOCK (ptr);

     type = heap->heapinfo[block].busy.type;
     switch (type) {
          case 0:
               /* Get as many statistics as early as we can.  */
               heap->chunks_used--;
               heap->bytes_used -= heap->heapinfo[block].busy.info.size * BLOCKSIZE;
               heap->bytes_free += heap->heapinfo[block].busy.info.size * BLOCKSIZE;

               /* Find the free cluster previous to this one in the free list.
                  Start searching at the last block referenced; this may benefit
                  programs with locality of allocation.  */
               i = heap->heapindex;
               if (i > block)
                    while (i > block)
                         i = heap->heapinfo[i].free.prev;
               else {
                    do
                         i = heap->heapinfo[i].free.next;
                    while (i > 0 && i < block);
                    i = heap->heapinfo[i].free.prev;
               }

               /* Determine how to link this block into the free list.  */
               if (block == i + heap->heapinfo[i].free.size) {
                    /* Coalesce this block with its predecessor.  */
                    heap->heapinfo[i].free.size += heap->heapinfo[block].busy.info.size;
                    block = i;
               }
               else {
                    /* Really link this block back into the free list.  */
                    heap->heapinfo[block].free.size = heap->heapinfo[block].busy.info.size;
                    heap->heapinfo[block].free.next = heap->heapinfo[i].free.next;
                    heap->heapinfo[block].free.prev = i;
                    heap->heapinfo[i].free.next = block;
                    heap->heapinfo[heap->heapinfo[block].free.next].free.prev = block;
                    heap->chunks_free++;
               }

               /* Now that the block is linked in, see if we can coalesce it
                  with its successor (by deleting its successor from the list
                  and adding in its size).  */
               if (block + heap->heapinfo[block].free.size == heap->heapinfo[block].free.next) {
                    heap->heapinfo[block].free.size
                    += heap->heapinfo[heap->heapinfo[block].free.next].free.size;
                    heap->heapinfo[block].free.next
                    = heap->heapinfo[heap->heapinfo[block].free.next].free.next;
                    heap->heapinfo[heap->heapinfo[block].free.next].free.prev = block;
                    heap->chunks_free--;
               }

               /* Now see if we can return stuff to the system.  */
               blocks = heap->heapinfo[block].free.size;
               if (blocks >= FINAL_FREE_BLOCKS && block + blocks == heap->heaplimit
                   && __shmalloc_brk( heap, 0 ) == ADDRESS (block + blocks)) {
                    register size_t bytes = blocks * BLOCKSIZE;
                    heap->heaplimit -= blocks;
                    __shmalloc_brk( heap, -bytes );
                    heap->heapinfo[heap->heapinfo[block].free.prev].free.next = heap->heapinfo[block].free.next;
                    heap->heapinfo[heap->heapinfo[block].free.next].free.prev = heap->heapinfo[block].free.prev;
                    block = heap->heapinfo[block].free.prev;
                    heap->chunks_free--;
                    heap->bytes_free -= bytes;
               }

               /* Set the next search to begin at this block.  */
               heap->heapindex = block;
               break;

          default:
               /* Do some of the statistics.  */
               heap->chunks_used--;
               heap->bytes_used -= 1 << type;
               heap->chunks_free++;
               heap->bytes_free += 1 << type;

               /* Get the address of the first free fragment in this block.  */
               prev = (struct list *) ((char *) ADDRESS (block) +
                                       (heap->heapinfo[block].busy.info.frag.first << type));

#if 1   /* Adapted from Mike */
               if ((int)heap->heapinfo[block].busy.info.frag.nfree == (BLOCKSIZE >> type) - 1
                   && heap->fragblocks[type] > 1)
#else
               if ((int)heap->heapinfo[block].busy.info.frag.nfree == (BLOCKSIZE >> type) - 1)
#endif
               {
                    /* If all fragments of this block are free, remove them
                       from the fragment list and free the whole block.  */
#if 1   /* Adapted from Mike */
                    heap->fragblocks[type]--;
#endif
                    next = prev;
                    for (i = 1; i < (size_t) (BLOCKSIZE >> type); ++i)
                         next = next->next;
                    prev->prev->next = next;
                    if (next != NULL)
                         next->prev = prev->prev;
                    heap->heapinfo[block].busy.type = 0;
                    heap->heapinfo[block].busy.info.size = 1;

                    /* Keep the statistics accurate.  */
                    heap->chunks_used++;
                    heap->bytes_used += BLOCKSIZE;
                    heap->chunks_free -= BLOCKSIZE >> type;
                    heap->bytes_free -= BLOCKSIZE;

                    _fusion_shfree( heap, ADDRESS (block) );
               }
               else if (heap->heapinfo[block].busy.info.frag.nfree != 0) {
                    /* If some fragments of this block are free, link this
                       fragment into the fragment list after the first free
                       fragment of this block. */
                    next = (struct list *) ptr;
                    next->next = prev->next;
                    next->prev = prev;
                    prev->next = next;
                    if (next->next != NULL)
                         next->next->prev = next;
                    heap->heapinfo[block].busy.info.frag.nfree++;
               }
               else {
                    /* No fragments of this block are free, so link this
                       fragment into the fragment list and announce that
                       it is the first free fragment of this block. */
                    prev = (struct list *) ptr;
                    heap->heapinfo[block].busy.info.frag.nfree = 1;
                    heap->heapinfo[block].busy.info.frag.first = (unsigned long int)
                                                                   ((unsigned long int) ((char *) ptr - (char *) NULL)
                                                                    % BLOCKSIZE >> type);
                    prev->next = heap->fraghead[type].next;
                    prev->prev = &heap->fraghead[type];
                    prev->prev->next = prev;
                    if (prev->next != NULL)
                         prev->next->prev = prev;
               }
               break;
     }
}

/**********************************************************************************************************************/

DirectResult
__shmalloc_init_heap( FusionSHM  *shm,
                      const char *filename,
                      void       *addr_base,
                      int         space,
                      int        *ret_fd,
                      int        *ret_size )
{
     DirectResult     ret;
     int              size;
     FusionSHMShared *shared;
     int              heapsize = (space + BLOCKSIZE-1) / BLOCKSIZE;
     bool             clear    = false;
     int              fd       = -1;
     shmalloc_heap   *heap     = NULL;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, '%s', %p, %p )\n",
                 __FUNCTION__, shm, filename, addr_base, ret_fd );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_ASSERT( filename != NULL );
     D_ASSERT( addr_base != NULL );
     D_ASSERT( ret_fd != NULL );
     D_ASSERT( ret_size != NULL );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );
     D_ASSERT( shared->tmpfs[0] != 0 );

     size = BLOCKALIGN(sizeof(shmalloc_heap)) + BLOCKALIGN( heapsize * sizeof(shmalloc_info) );


     D_DEBUG_AT( Fusion_SHMHeap, "  -> opening shared memory file '%s'...\n", filename );

     /* First remove potentially remaining file from a previous session. */
     if (unlink( filename ) && errno != ENOENT) {
          D_PERROR( "Fusion/SHM: Could not unlink '%s'! Erasing content instead...\n", filename );
          clear = true;
     }

     /* open the virtual file */
     fd = open( filename, O_RDWR | O_CREAT, 0660 );
     if (fd < 0) {
          ret = errno2result(errno);
          D_PERROR( "Fusion/SHM: Could not open shared memory file '%s'!\n", filename );
          goto error;
     }

     fchmod( fd, 0660 );
     ftruncate( fd, size );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> mmaping shared memory file... (%d bytes)\n", size );

     /* map it shared */
     heap = mmap( addr_base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0 );
     if (heap == MAP_FAILED) {
          ret = errno2result(errno);
          D_PERROR( "Fusion/SHM: Could not mmap shared memory file '%s'!\n", filename );
          goto error;
     }

     if (heap != addr_base) {
          D_ERROR( "Fusion/SHM: mmap() returned address (%p) differs from requested (%p)\n", heap, addr_base );
          ret = DFB_FUSION;
          goto error;
     }

     D_DEBUG_AT( Fusion_SHMHeap, "  -> done.\n" );

     if (clear)
          memset( heap, 0, size );

     heap->size     = size;
     heap->heapsize = heapsize;
     heap->heapinfo = (void*) heap + BLOCKALIGN(sizeof(shmalloc_heap));
     heap->heapbase = (char*) heap->heapinfo;

     D_MAGIC_SET( heap, shmalloc_heap );

     *ret_fd   = fd;
     *ret_size = size;

     return DFB_OK;


error:
     if (heap)
          munmap( heap, size );

     if (fd != -1) {
          close( fd );
          unlink( filename );
     }

     return ret;
}

DirectResult
__shmalloc_join_heap( FusionSHM  *shm,
                      const char *filename,
                      void       *addr_base,
                      int         size,
                      int        *ret_fd )
{
     DirectResult     ret;
     FusionSHMShared *shared;
     int              fd   = -1;
     shmalloc_heap   *heap = NULL;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, '%s', %p, %d, %p )\n",
                 __FUNCTION__, shm, filename, addr_base, size, ret_fd );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_ASSERT( filename != NULL );
     D_ASSERT( addr_base != NULL );
     D_ASSERT( size >= sizeof(shmalloc_heap) );
     D_ASSERT( ret_fd != NULL );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );
     D_ASSERT( shared->tmpfs[0] != 0 );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> opening shared memory file '%s'...\n", filename );

     /* open the virtual file */
     fd = open( filename, O_RDWR );
     if (fd < 0) {
          ret = errno2result(errno);
          D_PERROR( "Fusion/SHM: Could not open shared memory file '%s'!\n", filename );
          goto error;
     }

     D_DEBUG_AT( Fusion_SHMHeap, "  -> mmaping shared memory file... (%d bytes)\n", size );

     /* map it shared */
     heap = mmap( addr_base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0 );
     if (heap == MAP_FAILED) {
          ret = errno2result(errno);
          D_PERROR( "Fusion/SHM: Could not mmap shared memory file '%s'!\n", filename );
          goto error;
     }

     if (heap != addr_base) {
          D_ERROR( "Fusion/SHM: mmap() returned address (%p) differs from requested (%p)\n", heap, addr_base );
          ret = DFB_FUSION;
          goto error;
     }

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> done.\n" );

     *ret_fd = fd;

     return DFB_OK;


error:
     if (heap)
          munmap( heap, size );

     if (fd != -1)
          close( fd );

     return ret;
}

void *
__shmalloc_brk( shmalloc_heap *heap, int increment )
{
     FusionSHMShared     *shm;
     FusionWorld         *world;
     FusionSHMPool       *pool;
     FusionSHMPoolShared *shared;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %d )\n", __FUNCTION__, heap, increment );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     shared = heap->pool;

     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     shm = shared->shm;

     D_MAGIC_ASSERT( shm, FusionSHMShared );

     world = _fusion_world( shm->world );

     D_MAGIC_ASSERT( world, FusionWorld );

     pool = &world->shm.pools[shared->index];

     D_MAGIC_ASSERT( pool, FusionSHMPool );

     if (pool->size != heap->size) {
          D_WARN( "local pool size %d differs from shared heap size %d", pool->size, heap->size );

          if (!increment) {
               void *new_mem;

               new_mem = mremap( shared->addr_base, pool->size, heap->size, 0 );
               if (new_mem == MAP_FAILED) {
                    D_PERROR ("Fusion/SHM: mremapping shared memory file failed!\n");
                    return NULL;
               }

               D_DEBUG_AT( Fusion_SHMHeap, "  -> remapped (%d -> %d)\n", pool->size, heap->size );

               pool->size = heap->size;

               if (new_mem != shared->addr_base)
                    D_BREAK ("mremap returned a different address!");
          }
     }

     if (increment) {
          void                  *new_mem;
          int                    new_size = heap->size + increment;
          FusionSHMPoolDispatch  dispatch;

          if (new_size > shared->max_size) {
               D_WARN ("maximum shared memory size reached!");
               fusion_dbg_print_memleaks( shared );
               return NULL;
          }

          if (ftruncate( pool->fd, new_size ) < 0) {
               D_PERROR ("Fusion/SHM: ftruncating shared memory file failed!\n");
               return NULL;
          }

          new_mem = mremap( shared->addr_base, pool->size, new_size, 0 );
          if (new_mem == MAP_FAILED) {
               D_PERROR ("Fusion/SHM: mremapping shared memory file failed!\n");
               ftruncate( pool->fd, heap->size );
               return NULL;
          }

          D_DEBUG_AT( Fusion_SHMHeap, "  -> remapped (%d -> %d)\n", pool->size, new_size );

          heap->size = new_size;
          pool->size = new_size;

          if (new_mem != shared->addr_base)
               D_BREAK ("mremap returned a different address!");

          dispatch.pool_id = shared->pool_id;
          dispatch.size    = heap->size;

          while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DISPATCH, &dispatch )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_DISPATCH failed!\n" );
                    break;
               }
          }
     }

     return shared->addr_base + heap->size - increment;
}

