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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <misc/mem.h>

#include "fusion_types.h"

#include "fusion_internal.h"

#ifndef FUSION_FAKE

/*******************************
 *  Fusion internal functions  *
 *******************************/

AcquisitionStatus _shm_acquire (key_t key, int size, int *shmid)
{
     AcquisitionStatus result;

     /* first try to create a new shared memory segment */
     if ((*shmid = shmget (key, PAGE_ALIGN(size),
                           IPC_CREAT | IPC_EXCL | 0660)) < 0) {
          struct shmid_ds buf;

          /* try to get the existing shared memory segment */
          if ((*shmid = shmget (key, 0, 0)) < 0) {
               FPERROR ("shmget failed");
               return AS_Failure;
          }

          /* get information about existing segment */
          if (shmctl (*shmid, IPC_STAT, &buf) < 0) {
               FPERROR ("shmctl (IPC_STAT) failed");
               return AS_Failure;
          }

          /* if segment has another size than requested... */
          if (buf.shm_segsz != PAGE_ALIGN(size)) {
               /* if it's orphaned remove and recreate it, otherwise fail */
               if (buf.shm_nattch == 0) {
                    FDEBUG ("existing segment has a different size (recreating)\n");

                    if (shmctl (*shmid, IPC_RMID, NULL) < 0) {
                         FPERROR ("shmctl (IPC_RMID) failed");
                         return AS_Failure;
                    }

                    return _shm_acquire (key, size, shmid);
               }
               else {
                    FERROR ("** _shm_acquire: existing segment differs in size "
                            "but cannot be destroyed because it's used by others\n");
                    return AS_Failure;
               }
          }

          /* reinitialize orphaned segments */
          if (buf.shm_nattch == 0) {
               result = AS_Initialize;
               FDEBUG ("got existing segment (reinitializing)\n");
          }
          else {
               result = AS_Attach;
               FDEBUG ("got existing segment (attaching)\n");
          }
     }
     else {
          result = AS_Initialize;
          FDEBUG ("created new segment (initializing)\n");
     }

     return result;
}

AbolitionStatus _shm_abolish (int shmid, void *addr)
{
     struct shmid_ds buf;

     /* detach from segment first */
     if (shmdt (addr) < 0) {
          FPERROR ("shmdt failed");
          return AB_Failure;
     }

     /* get information about segment */
     if (shmctl (shmid, IPC_STAT, &buf) < 0) {
          FPERROR ("shmctl (IPC_STAT) failed");
          return AB_Failure;
     }

     /* if no one is attached anymore destroy it */
     if (buf.shm_nattch == 0) {
          FDEBUG ("destroying segment (no one attached)\n");

          if (shmctl (shmid, IPC_RMID, NULL) < 0) {
               FPERROR ("shmctl (IPC_RMID) failed");
               return AB_Failure;
          }

          return AB_Destroyed;
     }

     return AB_Detached;
}

#endif /* !FUSION_FAKE */

