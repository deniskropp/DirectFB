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
#include <signal.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>

#include "fusion_types.h"
#include "lock.h"

#include "fusion_internal.h"


/***************************
 *  Internal declarations  *
 ***************************/


/*******************
 *  Internal data  *
 *******************/



/****************
 *  Public API  *
 ****************/

int skirmish_init (FusionSkirmish *skirmish)
{
     union semun semopts;

     skirmish->sem_id = semget (IPC_PRIVATE, 1, IPC_CREAT | 0660);
     if (skirmish->sem_id < 0) {
          FPERROR("semget");

          return -1;
     }

     semopts.val = 1;
     if (semctl (skirmish->sem_id, 0, SETVAL, semopts)) {
          FPERROR("semctl");

          return -1;
     }

     return 0;
}

int skirmish_prevail (FusionSkirmish *skirmish)
{
     struct sembuf op;

     op.sem_num = 0;
     op.sem_op  = -1;
     op.sem_flg = SEM_UNDO;

     while (semop (skirmish->sem_id, &op, 1)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    FERROR ("skirmish already destroyed");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("semop");
          
          return FUSION_FAILURE;
     }

     return 0;
}

int skirmish_swoop (FusionSkirmish *skirmish)
{
     struct sembuf op;

     op.sem_num = 0;
     op.sem_op  = -1;
     op.sem_flg = SEM_UNDO | IPC_NOWAIT;

     while (semop (skirmish->sem_id, &op, 1)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    FERROR ("skirmish already destroyed");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("semop");
          
          return FUSION_FAILURE;
     }
     
     return FUSION_SUCCESS;
}

int skirmish_dismiss (FusionSkirmish *skirmish)
{
     struct sembuf op;

     op.sem_num = 0;
     op.sem_op  = 1;
     op.sem_flg = SEM_UNDO;

     while (semop (skirmish->sem_id, &op, 1)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    FERROR ("skirmish already destroyed");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("semop");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

void skirmish_destroy (FusionSkirmish *skirmish)
{
     union semun semopts;

     if (semctl (skirmish->sem_id, 0, IPC_RMID, semopts)) {
          FPERROR("semctl");
     }
}

/*******************************
 *  Fusion internal functions  *
 *******************************/



/*****************************
 *  File internal functions  *
 *****************************/


