/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <core/coredefs.h>

#include <misc/mem.h>

#include "fusion_types.h"

#include "fusion_internal.h"

#include "ref.h"


#ifndef FUSION_FAKE

#include "shmalloc/shmalloc_internal.h"

/***************************
 *  Internal declarations  *
 ***************************/

/*
 * shared fusion data
 */
typedef struct {
     long      next_fid;
     FusionRef ref;
} FusionShared;

/*
 * local fusion data
 */
typedef struct {
     int  fid;

     int  ref;

     int  shared_shm;
     int  shared_sem;

     FusionShared *shared;
} Fusion;


static void check_limits();
static int  initialize_shared (FusionShared *shared);

/*******************
 *  Internal data  *
 *******************/

/*
 *
 */
static Fusion *fusion = NULL;


/****************
 *  Public API  *
 ****************/

int
fusion_init()
{
     AcquisitionStatus as;

     /* check against multiple initialization */
     if (fusion) {
          /* increment local reference counter */
          fusion->ref++;

          return _fusion_id();
     }

     check_limits();

     /* allocate local Fusion data */
     fusion = DFBCALLOC (1, sizeof(Fusion));

     /* intialize local reference counter */
     fusion->ref = 1;

     /* acquire shared Fusion data */
     as = _shm_acquire (FUSION_KEY_PREFIX,
                        sizeof(FusionShared), &fusion->shared_shm);

     /* on failure free local Fusion data and return */
     if (as == AS_Failure) {
          DFBFREE (fusion);
          fusion = NULL;
          return -1;
     }

     fusion->shared = shmat (fusion->shared_shm, NULL, 0);

     /* initialize shared Fusion data? */
     if (as == AS_Initialize) {
          if (initialize_shared (fusion->shared)) {
               shmctl (fusion->shared_shm, IPC_RMID, NULL);
     
               DFBFREE (fusion);
               fusion = NULL;
               
               return -1;
          }
     }
     else {
          if (fusion_ref_up (&fusion->shared->ref, false)) {
               DFBFREE (fusion);
               fusion = NULL;
               
               return -1;
          }
     }

     /* set local Fusion ID */
     fusion->fid = fusion->shared->next_fid++;

     /* initialize shmalloc part */
     if (!__shmalloc_init(as == AS_Initialize)) {
          /* destroy shared Fusion data if we initialized it */
          if (as == AS_Initialize)
               shmctl (fusion->shared_shm, IPC_RMID, NULL);

          DFBFREE (fusion);
          fusion = NULL;
          
          return -1;
     }

     return _fusion_id();
}

void
fusion_exit()
{
     if (!fusion) {
          FERROR("called without being initialized!\n");
          return;
     }

     /* decrement local reference counter */
     if (--(fusion->ref))
          return;

     /* decrement shared reference counter */
     fusion_ref_down (&fusion->shared->ref, false);
     
     /* perform a shutdown? */
     if (fusion_ref_zero_trylock (&fusion->shared->ref) == FUSION_SUCCESS) {
          fusion_ref_destroy (&fusion->shared->ref);
     }

     switch (_shm_abolish (fusion->shared_shm, fusion->shared)) {
          case AB_Destroyed:
               FDEBUG ("I'VE BEEN THE LAST\n");
               __shmalloc_exit (true);
               break;

          case AB_Detached:
               FDEBUG ("OTHERS LEFT\n");
               __shmalloc_exit (false);
               break;

          case AB_Failure:
               FDEBUG ("UUUUUUUUH\n");
               break;
     }

     DFBFREE (fusion);
     fusion = NULL;
}


/*******************************
 *  Fusion internal functions  *
 *******************************/

int
_fusion_id()
{
     if (fusion)
          return fusion->fid;

     FERROR("called without being initialized!\n");

     return -1;
}


/*******************************
 *  File internal functions  *
 *******************************/

static void
check_msgmni()
{
     int     fd;
     int     msgmni;
     ssize_t len;
     char    buf[20];

     fd = open ("/proc/sys/kernel/msgmni", O_RDWR);
     if (fd < 0) {
          perror ("opening /proc/sys/kernel/msgmni");
          return;
     }

     len = read (fd, buf, 19);
     if (len < 1) {
          perror ("reading /proc/sys/kernel/msgmni");
          close (fd);
          return;
     }

     if (sscanf (buf, "%d", &msgmni)) {
          if (msgmni >= FUSION_MSGMNI) {
               close (fd);
               return;
          }
     }

     snprintf (buf, 19, "%d\n", FUSION_MSGMNI);

     if (write (fd, buf, strlen (buf) + 1) < 0)
          perror ("writing /proc/sys/kernel/msgmni");

     close (fd);
}

static void
check_sem()
{
     int     fd;
     int     per, sys, ops, num;
     ssize_t len;
     char    buf[40];

     fd = open ("/proc/sys/kernel/sem", O_RDWR);
     if (fd < 0) {
          perror ("opening /proc/sys/kernel/sem");
          return;
     }

     len = read (fd, buf, 39);
     if (len < 1) {
          perror ("reading /proc/sys/kernel/sem");
          close (fd);
          return;
     }

     if (sscanf (buf, "%d %d %d %d", &per, &sys, &ops, &num) < 4) {
          fprintf (stderr, "could not parse /proc/sys/kernel/sem\n");
          close (fd);
          return;
     }

     if (num >= FUSION_SEM_ARRAYS) {
          close (fd);
          return;
     }
     else
          num = FUSION_SEM_ARRAYS;

     snprintf (buf, 39, "%d %d %d %d\n", per, per * num, ops, num);

     if (write (fd, buf, strlen (buf) + 1) < 0)
          perror ("writing /proc/sys/kernel/sem");

     close (fd);
}

static void
check_limits()
{
     check_msgmni();
     check_sem();
}


static int
initialize_shared (FusionShared *shared)
{
     DFB_ASSERT( shared != NULL );

     shared->next_fid = 1;

     /* initialize shared reference counter */
     if (fusion_ref_init (&shared->ref))
          return -1;

     /* increment shared reference counter */
     if (fusion_ref_up (&shared->ref, false)) {
          fusion_ref_destroy (&shared->ref);

          return -2;
     }

     return 0;
}

#endif /* !FUSION_FAKE */

