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

/* Free a block of memory allocated by `malloc'.
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

#include "../shmalloc.h"
#include "shmalloc_internal.h"

/* Return memory to the heap.
   Like `shfree' but don't call a __shfree_hook if there is one.  */
void
_fusion_shfree_internal (void *ptr)
{
     int type;
     size_t block, blocks;
     register size_t i;
     struct list *prev, *next;

     block = BLOCK (ptr);

     type = _sheap->heapinfo[block].busy.type;
     switch (type) {
          case 0:
               /* Get as many statistics as early as we can.  */
               _sheap->chunks_used--;
               _sheap->bytes_used -= _sheap->heapinfo[block].busy.info.size * BLOCKSIZE;
               _sheap->bytes_free += _sheap->heapinfo[block].busy.info.size * BLOCKSIZE;

               /* Find the free cluster previous to this one in the free list.
                  Start searching at the last block referenced; this may benefit
                  programs with locality of allocation.  */
               i = _sheap->heapindex;
               if (i > block)
                    while (i > block)
                         i = _sheap->heapinfo[i].free.prev;
               else {
                    do
                         i = _sheap->heapinfo[i].free.next;
                    while (i > 0 && i < block);
                    i = _sheap->heapinfo[i].free.prev;
               }

               /* Determine how to link this block into the free list.  */
               if (block == i + _sheap->heapinfo[i].free.size) {
                    /* Coalesce this block with its predecessor.  */
                    _sheap->heapinfo[i].free.size += _sheap->heapinfo[block].busy.info.size;
                    block = i;
               }
               else {
                    /* Really link this block back into the free list.  */
                    _sheap->heapinfo[block].free.size = _sheap->heapinfo[block].busy.info.size;
                    _sheap->heapinfo[block].free.next = _sheap->heapinfo[i].free.next;
                    _sheap->heapinfo[block].free.prev = i;
                    _sheap->heapinfo[i].free.next = block;
                    _sheap->heapinfo[_sheap->heapinfo[block].free.next].free.prev = block;
                    _sheap->chunks_free++;
               }

               /* Now that the block is linked in, see if we can coalesce it
                  with its successor (by deleting its successor from the list
                  and adding in its size).  */
               if (block + _sheap->heapinfo[block].free.size == _sheap->heapinfo[block].free.next) {
                    _sheap->heapinfo[block].free.size
                    += _sheap->heapinfo[_sheap->heapinfo[block].free.next].free.size;
                    _sheap->heapinfo[block].free.next
                    = _sheap->heapinfo[_sheap->heapinfo[block].free.next].free.next;
                    _sheap->heapinfo[_sheap->heapinfo[block].free.next].free.prev = block;
                    _sheap->chunks_free--;
               }

               /* Now see if we can return stuff to the system.  */
               blocks = _sheap->heapinfo[block].free.size;
               if (blocks >= FINAL_FREE_BLOCKS && block + blocks == _sheap->heaplimit
                   && __shmalloc_brk (0) == ADDRESS (block + blocks)) {
                    register size_t bytes = blocks * BLOCKSIZE;
                    _sheap->heaplimit -= blocks;
                    __shmalloc_brk (-bytes);
                    _sheap->heapinfo[_sheap->heapinfo[block].free.prev].free.next
                    = _sheap->heapinfo[block].free.next;
                    _sheap->heapinfo[_sheap->heapinfo[block].free.next].free.prev
                    = _sheap->heapinfo[block].free.prev;
                    block = _sheap->heapinfo[block].free.prev;
                    _sheap->chunks_free--;
                    _sheap->bytes_free -= bytes;
               }

               /* Set the next search to begin at this block.  */
               _sheap->heapindex = block;
               break;

          default:
               /* Do some of the statistics.  */
               _sheap->chunks_used--;
               _sheap->bytes_used -= 1 << type;
               _sheap->chunks_free++;
               _sheap->bytes_free += 1 << type;

               /* Get the address of the first free fragment in this block.  */
               prev = (struct list *) ((char *) ADDRESS (block) +
                                       (_sheap->heapinfo[block].busy.info.frag.first << type));

#if 1   /* Adapted from Mike */
               if (_sheap->heapinfo[block].busy.info.frag.nfree == (BLOCKSIZE >> type) - 1
                   && _sheap->fragblocks[type] > 1)
#else
               if (_sheap->heapinfo[block].busy.info.frag.nfree == (BLOCKSIZE >> type) - 1)
#endif
               {
                    /* If all fragments of this block are free, remove them
                       from the fragment list and free the whole block.  */
#if 1   /* Adapted from Mike */
                    _sheap->fragblocks[type]--;
#endif
                    next = prev;
                    for (i = 1; i < (size_t) (BLOCKSIZE >> type); ++i)
                         next = next->next;
                    prev->prev->next = next;
                    if (next != NULL)
                         next->prev = prev->prev;
                    _sheap->heapinfo[block].busy.type = 0;
                    _sheap->heapinfo[block].busy.info.size = 1;

                    /* Keep the statistics accurate.  */
                    _sheap->chunks_used++;
                    _sheap->bytes_used += BLOCKSIZE;
                    _sheap->chunks_free -= BLOCKSIZE >> type;
                    _sheap->bytes_free -= BLOCKSIZE;

                    _fusion_shfree (ADDRESS (block));
               }
               else if (_sheap->heapinfo[block].busy.info.frag.nfree != 0) {
                    /* If some fragments of this block are free, link this
                       fragment into the fragment list after the first free
                       fragment of this block. */
                    next = (struct list *) ptr;
                    next->next = prev->next;
                    next->prev = prev;
                    prev->next = next;
                    if (next->next != NULL)
                         next->next->prev = next;
                    _sheap->heapinfo[block].busy.info.frag.nfree++;
               }
               else {
                    /* No fragments of this block are free, so link this
                       fragment into the fragment list and announce that
                       it is the first free fragment of this block. */
                    prev = (struct list *) ptr;
                    _sheap->heapinfo[block].busy.info.frag.nfree = 1;
                    _sheap->heapinfo[block].busy.info.frag.first = (unsigned long int)
                                                                   ((unsigned long int) ((char *) ptr - (char *) NULL)
                                                                    % BLOCKSIZE >> type);
                    prev->next = _sheap->fraghead[type].next;
                    prev->prev = &_sheap->fraghead[type];
                    prev->prev->next = prev;
                    if (prev->next != NULL)
                         prev->next->prev = prev;
               }
               break;
     }
}

/* Return memory to the heap.  */
void
_fusion_shfree (void *ptr)
{
     struct alignlist *l;

     if (ptr == NULL)
          return;

     for (l = _sheap->aligned_blocks; l != NULL; l = l->next)
          if (l->aligned == ptr) {
               l->aligned = NULL;      /* Mark the slot in the list as free.  */
               ptr = l->exact;
               break;
          }

     _fusion_shfree_internal (ptr);
}
