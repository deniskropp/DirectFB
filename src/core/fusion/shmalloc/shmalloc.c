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

/* Memory allocator `malloc'.
   Copyright 1990, 1991, 1992 Free Software Foundation
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

#include "shmalloc_internal.h"

shmalloc_heap *_sheap;

int __shmalloc_initialized;

/* Aligned allocation.  */
static void *
align (size_t size)
{
     void          *result;
     unsigned long  adj;

     result = __shmalloc_brk (size);

     adj = (unsigned long) result % BLOCKSIZE;
     if (adj != 0) {
          adj = BLOCKSIZE - adj;
          __shmalloc_brk (adj);
          result = (char *) result + adj;
     }

     return result;
}

/* Set everything up and remember that we have.  */
static int
initialize (void)
{
     if (_sheap)
          return 1;

     _sheap = __shmalloc_init(false);
     if (!_sheap)
          return 0;

     if (!_sheap->heapbase) {
          _sheap->heapsize = HEAP / BLOCKSIZE;

          _sheap->heapinfo = align (_sheap->heapsize * sizeof (shmalloc_info));
          if (!_sheap->heapinfo)
               return 0;

          memset (_sheap->heapinfo, 0, _sheap->heapsize * sizeof (shmalloc_info));

          /*
            _heapinfo[0].free.size = 0;
            _heapinfo[0].free.next = _heapinfo[0].free.prev = 0;
            _heapindex = 0;
          */

          _sheap->heapbase = (char *) _sheap->heapinfo;

          _sheap->reactor  = reactor_new (sizeof(int));
     }

     reactor_attach (_sheap->reactor, __shmalloc_react, NULL);

     __shmalloc_initialized = 1;

     return 1;
}

/* Get neatly aligned memory, initializing or
   growing the heap info table as necessary. */
static void *
morecore (size_t size)
{
     void *result;
     shmalloc_info *newinfo, *oldinfo;
     size_t newsize;

     result = align (size);
     if (result == NULL)
          return NULL;

     /* Check if we need to grow the info table.  */
     if ((size_t) BLOCK ((char *) result + size) > _sheap->heapsize) {
          newsize = _sheap->heapsize;

          while ((size_t) BLOCK ((char *) result + size) > newsize)
               newsize *= 2;

          newinfo = (shmalloc_info *) align (newsize * sizeof (shmalloc_info));
          if (newinfo == NULL) {
               __shmalloc_brk (-size);
               return NULL;
          }

          memset (newinfo, 0, newsize * sizeof (shmalloc_info));
          memcpy (newinfo, _sheap->heapinfo, _sheap->heapsize * sizeof (shmalloc_info));

          oldinfo = _sheap->heapinfo;

          newinfo[BLOCK (oldinfo)].busy.type = 0;
          newinfo[BLOCK (oldinfo)].busy.info.size
          = BLOCKIFY (_sheap->heapsize * sizeof (shmalloc_info));

          _sheap->heapinfo = newinfo;

          _shfree_internal (oldinfo);

          _sheap->heapsize = newsize;
     }

     _sheap->heaplimit = BLOCK ((char *) result + size);

     return result;
}

/* Allocate memory from the heap.  */
void *
shmalloc (size_t size)
{
     void *result;
     size_t block, blocks, lastblocks, start;
     register size_t i;
     struct list *next;

     /* Some programs will call shmalloc (0). We let them pass. */
#if 1
     if (size == 0)
          return NULL;
#endif

     if (!__shmalloc_initialized)
          if (!initialize ())
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
          next = _sheap->fraghead[log].next;
          if (next != NULL) {
               /* There are free fragments of this size.
                  Pop a fragment out of the fragment list and return it.
                  Update the block's nfree and first counters. */
               result = (void *) next;
               next->prev->next = next->next;
               if (next->next != NULL)
                    next->next->prev = next->prev;
               block = BLOCK (result);
               if (--(_sheap->heapinfo[block].busy.info.frag.nfree) != 0)
                    _sheap->heapinfo[block].busy.info.frag.first = (unsigned long int)
                                                                   ((unsigned long int) ((char *) next->next - (char *) NULL)
                                                                    % BLOCKSIZE) >> log;

               /* Update the statistics.  */
               _sheap->chunks_used++;
               _sheap->bytes_used += 1 << log;
               _sheap->chunks_free--;
               _sheap->bytes_free -= 1 << log;
          }
          else {
               /* No free fragments of the desired size, so get a new block
                  and break it into fragments, returning the first.  */
               result = shmalloc (BLOCKSIZE);
               if (result == NULL)
                    return NULL;
#if 1   /* Adapted from Mike */
               _sheap->fragblocks[log]++;
#endif

               /* Link all fragments but the first into the free list.  */
               for (i = 1; i < (size_t) (BLOCKSIZE >> log); ++i) {
                    next = (struct list *) ((char *) result + (i << log));
                    next->next = _sheap->fraghead[log].next;
                    next->prev = &_sheap->fraghead[log];
                    next->prev->next = next;
                    if (next->next != NULL)
                         next->next->prev = next;
               }

               /* Initialize the nfree and first counters for this block.  */
               block = BLOCK (result);
               _sheap->heapinfo[block].busy.type = log;
               _sheap->heapinfo[block].busy.info.frag.nfree = i - 1;
               _sheap->heapinfo[block].busy.info.frag.first = i - 1;

               _sheap->chunks_free += (BLOCKSIZE >> log) - 1;
               _sheap->bytes_free += BLOCKSIZE - (1 << log);
               _sheap->bytes_used -= BLOCKSIZE - (1 << log);
          }
     }
     else {
          /* Large allocation to receive one or more blocks.
             Search the free list in a circle starting at the last place visited.
             If we loop completely around without finding a large enough
             space we will have to get more memory from the system.  */
          blocks = BLOCKIFY (size);
          start = block = _sheap->heapindex;
          while (_sheap->heapinfo[block].free.size < blocks) {
               block = _sheap->heapinfo[block].free.next;
               if (block == start) {
                    /* Need to get more from the system.  Check to see if
                       the new core will be contiguous with the final free
                       block; if so we don't need to get as much.  */
                    block = _sheap->heapinfo[0].free.prev;
                    lastblocks = _sheap->heapinfo[block].free.size;
                    if (_sheap->heaplimit != 0 && block + lastblocks == _sheap->heaplimit &&
                        __shmalloc_brk (0) == ADDRESS (block + lastblocks) &&
                        (morecore ((blocks - lastblocks) * BLOCKSIZE)) != NULL) {
#if 1   /* Adapted from Mike */
                         /* Note that morecore() can change the location of
                            the final block if it moves the info table and the
                            old one gets coalesced into the final block. */
                         block = _sheap->heapinfo[0].free.prev;
                         _sheap->heapinfo[block].free.size += blocks - lastblocks;
#else
                         _sheap->heapinfo[block].free.size = blocks;
#endif
                         _sheap->bytes_free += (blocks - lastblocks) * BLOCKSIZE;
                         continue;
                    }
                    result = morecore (blocks * BLOCKSIZE);
                    if (result == NULL)
                         return NULL;
                    block = BLOCK (result);
                    _sheap->heapinfo[block].busy.type = 0;
                    _sheap->heapinfo[block].busy.info.size = blocks;
                    _sheap->chunks_used++;
                    _sheap->bytes_used += blocks * BLOCKSIZE;
                    return result;
               }
          }

          /* At this point we have found a suitable free list entry.
             Figure out how to remove what we need from the list. */
          result = ADDRESS (block);
          if (_sheap->heapinfo[block].free.size > blocks) {
               /* The block we found has a bit left over,
                  so relink the tail end back into the free list. */
               _sheap->heapinfo[block + blocks].free.size
               = _sheap->heapinfo[block].free.size - blocks;
               _sheap->heapinfo[block + blocks].free.next
               = _sheap->heapinfo[block].free.next;
               _sheap->heapinfo[block + blocks].free.prev
               = _sheap->heapinfo[block].free.prev;
               _sheap->heapinfo[_sheap->heapinfo[block].free.prev].free.next
               = _sheap->heapinfo[_sheap->heapinfo[block].free.next].free.prev
                 = _sheap->heapindex = block + blocks;
          }
          else {
               /* The block exactly matches our requirements,
                  so just remove it from the list. */
               _sheap->heapinfo[_sheap->heapinfo[block].free.next].free.prev
               = _sheap->heapinfo[block].free.prev;
               _sheap->heapinfo[_sheap->heapinfo[block].free.prev].free.next
               = _sheap->heapindex = _sheap->heapinfo[block].free.next;
               _sheap->chunks_free--;
          }

          _sheap->heapinfo[block].busy.type = 0;
          _sheap->heapinfo[block].busy.info.size = blocks;
          _sheap->chunks_used++;
          _sheap->bytes_used += blocks * BLOCKSIZE;
          _sheap->bytes_free -= blocks * BLOCKSIZE;
     }

     return result;
}
