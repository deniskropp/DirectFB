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


#ifndef FUSION_FAKE

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
          perror (__FUNCTION__": semget");

          return -1;
     }

     semopts.val = 1;
     if (semctl (skirmish->sem_id, 0, SETVAL, semopts)) {
          perror (__FUNCTION__": semctl");

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

     if (semop (skirmish->sem_id, &op, 1)) {
          perror (__FUNCTION__": semop");

          return -1;
     }

     return 0;
}

int skirmish_swoop (FusionSkirmish *skirmish)
{
     struct sembuf op;

     op.sem_num = 0;
     op.sem_op  = -1;
     op.sem_flg = SEM_UNDO | IPC_NOWAIT;

     return semop (skirmish->sem_id, &op, 1);
}

int skirmish_dismiss (FusionSkirmish *skirmish)
{
     struct sembuf op;

     op.sem_num = 0;
     op.sem_op  = 1;
     op.sem_flg = SEM_UNDO;

     if (semop (skirmish->sem_id, &op, 1)) {
          perror (__FUNCTION__": semop");

#ifdef FUSION_DEBUG
          fprintf (stderr, "semval: %d\n", semctl (skirmish->sem_id, 0, GETVAL, NULL));

          kill(getpid(), 5);
#endif

          return -1;
     }

     return 0;
}

void skirmish_destroy (FusionSkirmish *skirmish)
{
     union semun semopts;

     if (semctl (skirmish->sem_id, 0, IPC_RMID, semopts)) {
          perror (__FUNCTION__": semctl");
     }
}

/*******************************
 *  Fusion internal functions  *
 *******************************/



/*****************************
 *  File internal functions  *
 *****************************/


#endif /* !FUSION_FAKE */

