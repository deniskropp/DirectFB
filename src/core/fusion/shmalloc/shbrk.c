/*
   (c) Copyright 2001  Denis Oliver Kropp <dok@directfb.org>
   All rights reserved.

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

#include "shmalloc_internal.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define SH_NAME     "/dev/shm/fusion.shm"
#define SH_BASE     0x70000000
#define SH_MAX_SIZE 0x30000000

static int   fd   = -1;
static void *mem  = NULL;
static int   size = 0;

void *__shmalloc_init ()
{
     struct stat st;

     if (fd > -1) {
          //fprintf (stderr, __FUNCTION__ " called more than once!\n");
          return mem;
     }

     /* open the virtual file */
     fd = open (SH_NAME, O_RDWR | O_CREAT, 0600);
     if (fd < 0) {
          perror ("opening " SH_NAME);
          return NULL;
     }

     /* query size of memory */
     if (fstat (fd, &st) < 0) {
          perror ("fstating " SH_NAME);
          close (fd);
          fd = -1;
          return NULL;
     }

     /* if we are a slave */
     if (st.st_size)
          size = st.st_size;
     else {
          size = sizeof(shmalloc_heap);
          ftruncate (fd, size);
     }

     /* map it shared */
     mem = mmap ((void*) SH_BASE, size,
                 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
     if (mem == MAP_FAILED) {
          perror ("mmapping " SH_NAME);
          close (fd);
          fd = -1;
          return NULL;
     }

     return mem;
}

void *__shmalloc_brk (int increment)
{
     if (fd < 0) {
          fprintf (stderr, __FUNCTION__ " called without __shmalloc_init!\n");
          return NULL;
     }

     if (increment) {
          void *new_mem;
          int   new_size = size + increment;

          if (new_size > SH_MAX_SIZE)
               printf ("WARNING: maximum shared size exceeded!\n");

          if (ftruncate (fd, new_size) < 0) {
               perror ("ftruncating " SH_NAME);
               return NULL;
          }

          new_mem = mremap (mem, size, new_size, 0);
          if (new_mem == MAP_FAILED) {
               perror ("mremapping " SH_NAME);
               ftruncate (fd, size);
               return NULL;
          }

          if (new_mem != mem)
               printf ("FATAL: mremap returned a different address!\n");

          size = new_size;

          if (_sheap && _sheap->reactor)
               reactor_dispatch (_sheap->reactor, (const void *) &size, false);
     }

     return mem + size - increment;
}

ReactionResult __shmalloc_react (const void *msg_data, void *ctx)
{
     void *new_mem;
     int   new_size = *((int*) msg_data);

     new_mem = mremap (mem, size, new_size, 0);
     if (new_mem == MAP_FAILED) {
          perror ("FATAL: mremap in __shmalloc_react failed" SH_NAME);
          return RS_OK;
     }

     if (new_mem != mem)
          printf ("FATAL: mremap returned a different address!\n");

     size = new_size;

     return RS_OK;
}

