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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define SH_BASE          0x70000000
#define SH_MAX_SIZE      0x30000000
#define SH_FILE_NAME     "/fusion.shm"
#define SH_DEFAULT_NAME  "/dev/shm" SH_FILE_NAME
#define SH_MOUNTS_FILE   "/proc/mounts"
#define SH_SHMFS_TYPE    "shm"
#define SH_BUFSIZE       256

static int   fd            = -1;
static void *mem           = NULL;
static int   size          = 0;
static char  default_name[]= SH_DEFAULT_NAME;

static char *
shmalloc_check_shmfs (void)
{
     FILE * mounts_handle = NULL;
     char * pointer       = NULL;
     char * mount_point   = NULL;
     char * mount_fs      = NULL;

     char   buffer[SH_BUFSIZE];

     //fprintf (stderr, "default-shmfs-name: %s\n", default_name);

     if (!(mounts_handle = fopen (SH_MOUNTS_FILE, "r"))) {
          return(default_name);
     }

     while (fgets (buffer, SH_BUFSIZE, mounts_handle)) {
          pointer = buffer;
          strsep (&pointer, " ");
          mount_point = strsep (&pointer, " ");
          mount_fs = strsep (&pointer, " ");
          if (mount_fs && mount_point \
              && (strlen (mount_fs) == strlen (SH_SHMFS_TYPE)) \
              && (!(strcmp (mount_fs, SH_SHMFS_TYPE)))) {
               if (!(pointer = (char *) malloc (strlen (mount_point) \
                                                + strlen (SH_FILE_NAME) + 1))) {
                    fclose (mounts_handle);
                    return(default_name);
               }
               strcpy (pointer, mount_point);
               strcat (pointer, SH_FILE_NAME);
               fclose (mounts_handle);
               return(pointer);
          }
     }

     fclose (mounts_handle);
     return(default_name);
}

void *__shmalloc_init (bool initialize)
{
     struct stat   st;
     char        * sh_name = NULL;

     if (mem) {
          //fprintf (stderr, __FUNCTION__ " called more than once!\n");
          return mem;
     }

     /* try to find out where the shmfs is actually mounted */

     sh_name = shmalloc_check_shmfs ();
     //fprintf (stderr, "shmfs-filename: %s\n", sh_name);

     /* open the virtual file */
     if (initialize)
          fd = open (sh_name, O_RDWR | O_CREAT, 0600);
     else
          fd = open (sh_name, O_RDWR, 0600);
     if (fd < 0) {
          perror ("opening shared memory file");
          return NULL;
     }

     /* init or join */
     if (initialize) {
          size = sizeof(shmalloc_heap);
          ftruncate (fd, size);
     }
     else {
          size = st.st_size;
          
          /* query size of memory */
          if (fstat (fd, &st) < 0) {
               perror ("fstating shared memory file");
               close (fd);
               fd = -1;
               return NULL;
          }
     }

     /* map it shared */
     mem = mmap ((void*) SH_BASE, size,
                 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
     if (mem == MAP_FAILED) {
          perror ("mmapping shared memory file");
          close (fd);
          fd  = -1;
          mem = NULL;
     }

     /* clear it */
     if (initialize)
          memset (mem, 0, size);

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
               perror ("ftruncating shared memory file");
               return NULL;
          }

          new_mem = mremap (mem, size, new_size, 0);
          if (new_mem == MAP_FAILED) {
               perror ("mremapping shared memory file");
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
          perror ("FATAL: mremap in __shmalloc_react failed on shared memory file");
          return RS_OK;
     }

     if (new_mem != mem)
          printf ("FATAL: mremap returned a different address!\n");

     size = new_size;

     return RS_OK;
}

