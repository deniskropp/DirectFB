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

/* Change the size of a block allocated by `malloc'.
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

#include "../shmalloc.h"
#include "shmalloc_internal.h"

#include <string.h>

#define min(A, B) ((A) < (B) ? (A) : (B))


/* Resize the given region to the new size, returning a pointer
   to the (possibly moved) region.  This is optimized for speed;
   some benchmarks seem to indicate that greater compactness is
   achieved by unconditionally allocating and copying to a
   new region.  This module has incestuous knowledge of the
   internals of both free and shmalloc. */
void *
_fusion_shrealloc (void *ptr, size_t size)
{
     void  *result;
     int    type;
     size_t block, blocks, oldlimit;

     if (ptr == NULL)
          return _fusion_shmalloc (size);
     else if (size == 0) {
          _fusion_shfree (ptr);
          return _fusion_shmalloc (0);
     }

     block = BLOCK (ptr);

     type = _sheap->heapinfo[block].busy.type;
     switch (type) {
          case 0:
               /* Maybe reallocate a large block to a small fragment.  */
               if (size <= BLOCKSIZE / 2) {
                    result = _fusion_shmalloc (size);
                    if (result != NULL) {
                         memcpy (result, ptr, size);
                         _fusion_shfree (ptr);
                         return result;
                    }
               }

               /* The new size is a large allocation as well;
                  see if we can hold it in place. */
               blocks = BLOCKIFY (size);
               if (blocks < _sheap->heapinfo[block].busy.info.size) {
                    /* The new size is smaller; return
                       excess memory to the free list. */
                    _sheap->heapinfo[block + blocks].busy.type = 0;
                    _sheap->heapinfo[block + blocks].busy.info.size
                    = _sheap->heapinfo[block].busy.info.size - blocks;
                    _sheap->heapinfo[block].busy.info.size = blocks;
                    _fusion_shfree (ADDRESS (block + blocks));
                    result = ptr;
               }
               else if (blocks == _sheap->heapinfo[block].busy.info.size)
                    /* No size change necessary.  */
                    result = ptr;
               else {
                    /* Won't fit, so allocate a new region that will.
                       Free the old region first in case there is sufficient
                       adjacent free space to grow without moving. */
                    blocks = _sheap->heapinfo[block].busy.info.size;
                    /* Prevent free from actually returning memory to the system.  */
                    oldlimit = _sheap->heaplimit;
                    _sheap->heaplimit = 0;
                    _fusion_shfree (ptr);
                    _sheap->heaplimit = oldlimit;
                    result = _fusion_shmalloc (size);
                    if (result == NULL) {
                         /* Now we're really in trouble.  We have to unfree
                            the thing we just freed.  Unfortunately it might
                            have been coalesced with its neighbors.  */
                         if (_sheap->heapindex == block)
                              (void) _fusion_shmalloc (blocks * BLOCKSIZE);
                         else {
                              void *previous = _fusion_shmalloc ((block - _sheap->heapindex) * BLOCKSIZE);
                              (void) _fusion_shmalloc (blocks * BLOCKSIZE);
                              _fusion_shfree (previous);
                         }
                         return NULL;
                    }
                    if (ptr != result)
                         memmove (result, ptr, blocks * BLOCKSIZE);
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
                    result = _fusion_shmalloc (size);
                    if (result == NULL)
                         return NULL;
                    memcpy (result, ptr, min (size, (size_t) 1 << type));
                    _fusion_shfree (ptr);
               }
               break;
     }

     return result;
}
