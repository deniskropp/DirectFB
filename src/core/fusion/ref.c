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

FusionResult ref_init (FusionRef *ref)
{
  union semun semopts;

  /* create two semaphores, one for locking, one for counting */
  ref->sem_id = semget (IPC_PRIVATE, 2, IPC_CREAT | 0660);
  if (ref->sem_id < 0)
    {
      FPERROR ("semget");

      if (errno == ENOMEM || errno == ENOSPC)
        return FUSION_LIMITREACHED;

      return FUSION_FAILURE;
    }

  /* initialize the lock */
  semopts.val = 1;
  if (semctl (ref->sem_id, 0, SETVAL, semopts))
    {
      FPERROR ("semctl");

      semctl (ref->sem_id, 0, IPC_RMID, 0);
      return FUSION_FAILURE;
    }

  return FUSION_SUCCESS;
}

FusionResult ref_up (FusionRef *ref)
{
  struct sembuf op[2];

  /* lock/increase */
  op[0].sem_num = 0;
  op[0].sem_op  = -1;
  op[0].sem_flg = SEM_UNDO;
  op[1].sem_num = 1;
  op[1].sem_op  = 1;
  op[1].sem_flg = SEM_UNDO;

  while (semop (ref->sem_id, op, 2))
    {
      FPERROR ("semop");

      switch (errno)
        {
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

  while (semop (ref->sem_id, op, 1))
    {
      FPERROR ("semop");

      switch (errno)
        {
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

FusionResult ref_down (FusionRef *ref)
{
  struct sembuf op[2];

  /* lock/decrease */
  op[0].sem_num = 0;
  op[0].sem_op  = -1;
  op[0].sem_flg = SEM_UNDO;
  op[1].sem_num = 1;
  op[1].sem_op  = -1;
  op[1].sem_flg = SEM_UNDO;

  while (semop (ref->sem_id, op, 2))
    {
      FPERROR ("semop");

      switch (errno)
        {
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

  while (semop (ref->sem_id, op, 1))
    {
      FPERROR ("semop");

      switch (errno)
        {
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

FusionResult ref_zero_lock (FusionRef *ref)
{
  struct sembuf op[2];

  /* wait for zero / lock */
  op[0].sem_num = 1;
  op[0].sem_op  = 0;
  op[0].sem_flg = 0;
  op[1].sem_num = 0;
  op[1].sem_op  = -1;
  op[1].sem_flg = SEM_UNDO;

  while (semop (ref->sem_id, op, 2))
    {
      FPERROR ("semop");

      switch (errno)
        {
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

FusionResult ref_zero_trylock (FusionRef *ref)
{
  struct sembuf op[2];

  /* check for zero / lock */
  op[0].sem_num = 1;
  op[0].sem_op  = 0;
  op[0].sem_flg = IPC_NOWAIT;
  op[1].sem_num = 0;
  op[1].sem_op  = -1;
  op[1].sem_flg = SEM_UNDO;

  while (semop (ref->sem_id, op, 2))
    {
      if (errno != EAGAIN)
        FPERROR ("semop");

      switch (errno)
        {
        case EINTR:
          continue;
        case EAGAIN:
          return FUSION_INUSE;
        case EACCES:
          return FUSION_ACCESSDENIED;
        case EIDRM:
          return FUSION_DESTROYED;
        }

      return FUSION_FAILURE;
    }

  return FUSION_SUCCESS;
}

FusionResult ref_unlock (FusionRef *ref)
{
  struct sembuf op;

  /* unlock */
  op.sem_num = 0;
  op.sem_op  = 1;
  op.sem_flg = SEM_UNDO;

  while (semop (ref->sem_id, &op, 1))
    {
      FPERROR ("semop");

      switch (errno)
        {
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

FusionResult ref_destroy (FusionRef *ref)
{
  union semun semopts;
  
  if (semctl (ref->sem_id, 0, IPC_RMID, semopts))
    {
      FPERROR ("semctl");

      switch (errno)
        {
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

/*******************************
 *  Fusion internal functions  *
 *******************************/



/*****************************
 *  File internal functions  *
 *****************************/

