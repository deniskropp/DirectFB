/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <signal.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <fusion/fusion_internal.h>

#include "shmalloc_internal.h"

#define SH_BASE          0x20000000
#define SH_MAX_SIZE      0x20000000
#define SH_FILE_NAME     "/fusion."
#define SH_MOUNTS_FILE   "/proc/mounts"
#define SH_BUFSIZE       1024

static int   fd            = -1;
static void *mem           = NULL;
static int   size          = 0;
static char *sh_name       = NULL;

shmalloc_heap   *_fusion_shmalloc_heap = NULL;
static Reaction  reaction;

static char *
shmalloc_check_shmfs (int world)
{
     FILE * mounts_handle = NULL;
     char * pointer       = NULL;
     char * mount_point   = NULL;
     char * mount_fs      = NULL;

     int    largest = 0;
     char  *name    = NULL;

     char   buffer[SH_BUFSIZE];

     if (!(mounts_handle = fopen (SH_MOUNTS_FILE, "r")))
          return NULL;

     while (fgets (buffer, SH_BUFSIZE, mounts_handle)) {
          pointer = buffer;

          strsep (&pointer, " ");

          mount_point = strsep (&pointer, " ");
          mount_fs = strsep (&pointer, " ");

          if (mount_fs && mount_point &&
              (!strcmp (mount_fs, "tmpfs") || !strcmp (mount_fs, "shmfs")))
          {
               struct statfs  stat;
               int            bytes;
               int            len = strlen (mount_point) + strlen (SH_FILE_NAME) + 11;

               if (statfs (mount_point, &stat)) {
                    D_PERROR ("Fusion/SHM: statfs on tmpfs failed!\n");
                    continue;
               }

               bytes = stat.f_blocks * stat.f_bsize;

               if (bytes <= largest)
                    continue;

               if (name)
                    D_FREE( name );

               name = D_MALLOC( len );
               if (!name) {
                    D_OOM();
                    fclose (mounts_handle);
                    return NULL;
               }

               snprintf (name, len, "%s%s%d", mount_point, SH_FILE_NAME, world);

               largest = bytes;
          }
     }

     fclose (mounts_handle);

     return name;
}

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

          direct_memcpy (newinfo, _sheap->heapinfo,
                         _sheap->heapsize * sizeof (shmalloc_info));

          memset (newinfo + _sheap->heapsize,
                  0, (newsize - _sheap->heapsize) * sizeof (shmalloc_info));

          oldinfo = _sheap->heapinfo;

          newinfo[BLOCK (oldinfo)].busy.type = 0;
          newinfo[BLOCK (oldinfo)].busy.info.size = BLOCKIFY (_sheap->heapsize * sizeof (shmalloc_info));

          _sheap->heapinfo = newinfo;

          _fusion_shfree_internal (oldinfo);

          _sheap->heapsize = newsize;
     }

     _sheap->heaplimit = BLOCK ((char *) result + size);

     return result;
}

/* Allocate memory from the heap.  */
void *
_fusion_shmalloc (size_t size)
{
     void *result;
     size_t block, blocks, lastblocks, start;
     register size_t i;
     struct list *next;

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
               result = _fusion_shmalloc (BLOCKSIZE);
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


/******************************************************************************/

void *
__shmalloc_init (int world, bool initialize)
{
     struct stat st;

     if (mem)
          return mem;

     if (fusion_config->tmpfs) {
          int len = strlen (fusion_config->tmpfs) + strlen (SH_FILE_NAME) + 11;

          if (!(sh_name = D_MALLOC(len))) {
               D_ERROR ("Fusion/SHM: malloc failed!\n");
               return NULL;
          }

          snprintf (sh_name, len, "%s%s%d", fusion_config->tmpfs, SH_FILE_NAME, world);
     }
     else {
          /* try to find out where the tmpfs is actually mounted */
          sh_name = shmalloc_check_shmfs (world);
          if (!sh_name) {
               D_ERROR ("Fusion/SHM: Could not find tmpfs mount point!\n");
               return NULL;
          }
     }

     /* open the virtual file */
     if (initialize)
          fd = open (sh_name, O_RDWR | O_CREAT, 0660);
     else
          fd = open (sh_name, O_RDWR);
     if (fd < 0) {
          D_PERROR ("Fusion/SHM: opening shared memory file failed!\n");
          D_FREE( sh_name );
          return NULL;
     }

     /* init or join */
     if (initialize) {
          chmod (sh_name, 0660);

          size = sizeof(shmalloc_heap);
          ftruncate (fd, size);
     }
     else {
          /* query size of memory */
          if (fstat (fd, &st) < 0) {
               D_PERROR ("Fusion/SHM: fstat()ing shared memory file failed!\n");
               close (fd);
               fd = -1;
               D_FREE( sh_name );
               return NULL;
          }

          size = st.st_size;
     }

     D_DEBUG("Fusion/SHM: mmaping shared memory file...\n");

     /* map it shared */
     mem = mmap ((void*) SH_BASE, size,
                 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
     if (mem == MAP_FAILED) {
          D_PERROR ("Fusion/SHM: mmapping shared memory file failed!\n");
          close (fd);
          fd  = -1;
          mem = NULL;
     }

     D_DEBUG("Fusion/SHM: mmapped shared memory file.\n");

     _sheap = mem;

     if (initialize) {
          memset (mem, 0, size);

          _sheap->heapsize = HEAP / BLOCKSIZE;

          _sheap->heapinfo = align (_sheap->heapsize * sizeof (shmalloc_info));
          if (!_sheap->heapinfo) {
               D_ERROR ("Fusion/SHM: Could not allocate _sheap->heapinfo!\n");
               _sheap = NULL;
               D_FREE( sh_name );
               return NULL;
          }

          memset (_sheap->heapinfo, 0, _sheap->heapsize * sizeof (shmalloc_info));

          _sheap->heapinfo[0].free.size = 0;
          _sheap->heapinfo[0].free.next = _sheap->heapinfo[0].free.prev = 0;
          _sheap->heapindex = 0;

          _sheap->heapbase = (char *) _sheap->heapinfo;

          _sheap->map_size = size;

          fusion_skirmish_init( &_sheap->lock, "Shared Memory Heap" );
     }

     return mem;
}

void
__shmalloc_attach()
{
     D_ASSERT( _sheap != NULL );
     D_ASSERT( _sheap->reactor != NULL );

     fusion_reactor_attach( _sheap->reactor, __shmalloc_react, NULL, &reaction );
}

void *
__shmalloc_brk (int increment)
{
     D_DEBUG( "Fusion/SHM: __shmalloc_brk( %d + %d -> %d )\n", size, increment, size + increment );

     D_ASSERT( fd >= 0 );

     if (increment) {
          void *new_mem;
          int   new_size = size + increment;

          if (new_size > SH_MAX_SIZE) {
               D_WARN ("maximum shared memory size reached!");
               return NULL;
          }

          if (ftruncate (fd, new_size) < 0) {
               D_PERROR ("Fusion/SHM: ftruncating shared memory file failed!\n");
               return NULL;
          }

          new_mem = mremap (mem, size, new_size, 0);
          if (new_mem == MAP_FAILED) {
               D_PERROR ("Fusion/SHM: mremapping shared memory file failed!\n");
               ftruncate (fd, size);
               return NULL;
          }

          size = new_size;

          _sheap->map_size = size;

          if (new_mem != mem)
               D_BREAK ("mremap returned a different address!");

          if (_sheap && _sheap->reactor)
               fusion_reactor_dispatch( _sheap->reactor, (const void *) &size, false, NULL );
     }

     return mem + size - increment;
}

ReactionResult __shmalloc_react (const void *msg_data, void *ctx)
{
     struct stat  st;
     void        *new_mem;
     const long  *size_msg = msg_data;

     (void) size_msg;

     D_ASSERT( msg_data != NULL );

     D_ASSERT( fd >= 0 );

     /* Query size of file. */
     if (fstat (fd, &st) < 0) {
          D_PERROR( "Fusion/SHM: fstat() on shared memory file failed!\n" );
          return RS_OK;
     }

     D_ASSUME( st.st_size == *size_msg );

     if (size != st.st_size) {
          D_DEBUG("Fusion/SHM: __shmalloc_react (%d -> %ld)\n", size, st.st_size );

          new_mem = mremap (mem, size, st.st_size, 0);
          if (new_mem == MAP_FAILED) {
               D_PERROR ("Fusion/SHM: mremap on shared memory file failed!\n");
               return RS_OK;
          }

          if (new_mem != mem)
               D_BREAK ("mremap returned a different address");

          size = st.st_size;
     }

     return RS_OK;
}

void __shmalloc_exit (bool shutdown, bool detach)
{
     D_ASSUME( mem != NULL );
     D_ASSUME( _sheap != NULL );

     if (!mem)
          return;

     if (_sheap) {
          FusionReactor *reactor = _sheap->reactor;

          /* Detach from reactor */
          if (detach) {
               D_ASSERT( reactor != NULL );

               fusion_reactor_detach (_sheap->reactor, &reaction);
          }

          /* Destroy reactor & skirmish */
          if (shutdown) {
               D_ASSERT( reactor != NULL );

               /* Avoid further dispatching by following free() calls */
               _sheap->reactor = NULL;

               fusion_reactor_free( reactor );

               if (_sheap->root_node)
                    SHFREE( _sheap->root_node );

               fusion_dbg_print_memleaks();

               fusion_skirmish_destroy( &_sheap->lock );
          }

          _sheap = NULL;
     }

     munmap( mem, size );
     mem = NULL;

     close( fd );
     fd = -1;

     if (shutdown)
          unlink( sh_name );

     D_FREE( sh_name );
     sh_name = NULL;
}

void *__shmalloc_allocate_root( size_t size )
{
     D_ASSERT( _sheap != NULL );
     D_ASSERT( _sheap->root_node == NULL );

     _sheap->root_node = SHCALLOC( 1, size );

     return _sheap->root_node;
}

void *__shmalloc_get_root()
{
     D_ASSERT( _sheap != NULL );

     return _sheap->root_node;
}

bool fusion_is_shared( const void *ptr )
{
     return((unsigned int) ptr >= SH_BASE &&
            (unsigned int) ptr <  SH_BASE + SH_MAX_SIZE);
}

bool
fusion_shmalloc_cure (const void *ptr)
{
     struct stat st;
     int         offset = (int) ptr - SH_BASE;

     D_DEBUG ("Fusion/SHM: trying to cure segfault at address %p...\n", ptr );

     /* Check file descriptor. */
     if (fd < 0) {
          D_DEBUG ("Fusion/SHM:   won't cure, shared memory file is not opened.\n" );
          return false;
     }

     /* Check address space. */
     if (offset < 0 || offset >= SH_MAX_SIZE) {
          D_DEBUG ("Fusion/SHM:   won't cure, address is outside shared address space.\n" );
          return false;
     }

     /* Shouldn't happen, but... */
     if (offset < size) {
          D_DEBUG ("Fusion/SHM:   won't cure, address is inside mapped region!?\n" );
          return false;
     }

     /* Query size of memory. */
     if (fstat (fd, &st) < 0) {
          D_PERROR ("Fusion/SHM: fstating shared memory file failed!\n" );
          return false;
     }

     /* Check for pending remap. */
     if (st.st_size != size) {
          D_DEBUG ("Fusion/SHM:   pending remap (%d -> %ld)...\n", size, st.st_size );

          if (offset < st.st_size) {
               void *new_mem;

               D_DEBUG ("Fusion/SHM:   address is inside new region, remapping...\n" );

               new_mem = mremap (mem, size, st.st_size, 0);
               if (new_mem == MAP_FAILED) {
                    D_PERROR ("Fusion/SHM: mremap on shared memory file failed!\n");
                    return false;
               }

               if (new_mem != mem)
                    D_BREAK ("mremap returned a different address");

               size = st.st_size;

               D_DEBUG ("Fusion/SHM:   successfully cured ;)\n" );

               return true;
          }

          D_DEBUG ("Fusion/SHM:   address is even outside new region, cannot cure ;(\n" );
     }
     else
          D_DEBUG ("Fusion/SHM:   no pending remap, cannot cure ;(\n" );

     return false;
}

