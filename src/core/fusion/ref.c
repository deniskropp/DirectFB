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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>

#include "fusion_types.h"
#include "ref.h"

#include "fusion_internal.h"

#include <signal.h>
/***************************
 *  Internal declarations  *
 ***************************/


/*******************
 *  Internal data  *
 *******************/



/****************
 *  Public API  *
 ****************/
 
#ifndef FUSION_FAKE

FusionResult fusion_ref_init (FusionRef *ref)
{
     union semun semopts;

     /* create two semaphores, one for locking, one for counting */
     ref->sem_id = semget (IPC_PRIVATE, 2, IPC_CREAT | 0660);
     if (ref->sem_id < 0) {
          FPERROR ("semget");

          if (errno == ENOMEM || errno == ENOSPC)
               return FUSION_LIMITREACHED;

          return FUSION_FAILURE;
     }

     /* initialize the lock */
     semopts.val = 1;
     if (semctl (ref->sem_id, 0, SETVAL, semopts)) {
          FPERROR ("semctl");

          semctl (ref->sem_id, 0, IPC_RMID, 0);
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_up (FusionRef *ref, bool global)
{
     struct sembuf op[2];

     /* lock/increase */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;
     op[1].sem_num = 1;
     op[1].sem_op  = 1;
     if (global)
          op[1].sem_flg = 0;
     else
          op[1].sem_flg = SEM_UNDO;

     while (semop (ref->sem_id, op, 2)) {
          FPERROR ("semop");

          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     /* unlock */
     op[0].sem_op  = 1;

     while (semop (ref->sem_id, op, 1)) {
          FPERROR ("semop");

          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_down (FusionRef *ref, bool global)
{
     struct sembuf op[2];

     /* lock/decrease */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;
     op[1].sem_num = 1;
     op[1].sem_op  = -1;
     if (global)
          op[1].sem_flg = 0;
     else
          op[1].sem_flg = SEM_UNDO;

     while (semop (ref->sem_id, op, 2)) {
          FPERROR ("semop");

          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     /* unlock */
     op[0].sem_op  = 1;

     while (semop (ref->sem_id, op, 1)) {
          FPERROR ("semop");

          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_zero_lock (FusionRef *ref)
{
     struct sembuf op[2];

     /* wait for zero / lock */
     op[0].sem_num = 1;
     op[0].sem_op  = 0;
     op[0].sem_flg = 0;
     op[1].sem_num = 0;
     op[1].sem_op  = -1;
     op[1].sem_flg = SEM_UNDO;

     while (semop (ref->sem_id, op, 2)) {
          FPERROR ("semop");

          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_zero_trylock (FusionRef *ref)
{
     struct sembuf op[2];

     /* check for zero / lock */
     op[0].sem_num = 1;
     op[0].sem_op  = 0;
     op[0].sem_flg = IPC_NOWAIT;
     op[1].sem_num = 0;
     op[1].sem_op  = -1;
     op[1].sem_flg = SEM_UNDO;

     while (semop (ref->sem_id, op, 2)) {
          if (errno != EAGAIN)
               FPERROR ("semop");

          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EINVAL:
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_unlock (FusionRef *ref)
{
     struct sembuf op;

     /* unlock */
     op.sem_num = 0;
     op.sem_op  = 1;
     op.sem_flg = SEM_UNDO;

     while (semop (ref->sem_id, &op, 1)) {
          FPERROR ("semop");

          switch (errno) {
               case EINTR:
                    continue;
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_destroy (FusionRef *ref)
{
     union semun semopts;

     if (semctl (ref->sem_id, 0, IPC_RMID, semopts)) {
          FPERROR ("semctl");

          switch (errno) {
               case EACCES:
                    return FUSION_ACCESSDENIED;
               case EPERM:
                    return FUSION_PERMISSIONDENIED;
               case EIDRM:
                    return FUSION_DESTROYED;
          }

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

#else /* !FUSION_FAKE */

FusionResult fusion_ref_init (FusionRef *ref)
{
     pthread_mutex_init (&ref->lock, NULL);

     ref->refs      = 0;
     ref->destroyed = false;

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_up (FusionRef *ref, bool global)
{
     FusionResult ret = FUSION_SUCCESS;

     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else
          ref->refs++;
     
     pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult fusion_ref_down (FusionRef *ref, bool global)
{
     FusionResult ret = FUSION_SUCCESS;

     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else
          ref->refs--;
     
     pthread_cond_broadcast (&ref->cond);
     
     pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult fusion_ref_zero_lock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else while (ref->refs && !ret) {
          pthread_cond_wait (&ref->cond, &ref->lock);
          
          if (ref->destroyed)
               ret = FUSION_DESTROYED;
     }
     
     if (ret != FUSION_SUCCESS)
          pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult fusion_ref_zero_trylock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else if (ref->refs)
          ret = FUSION_INUSE;
     
     if (ret != FUSION_SUCCESS)
          pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult fusion_ref_unlock (FusionRef *ref)
{
     pthread_mutex_unlock (&ref->lock);

     return FUSION_SUCCESS;
}

FusionResult fusion_ref_destroy (FusionRef *ref)
{
     ref->destroyed = true;

     pthread_cond_broadcast (&ref->cond);

     pthread_mutex_unlock (&ref->lock);
     
     return FUSION_SUCCESS;
}

#endif

/*******************************
 *  Fusion internal functions  *
 *******************************/



/*****************************
 *  File internal functions  *
 *****************************/

