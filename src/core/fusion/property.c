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

#include <core/coredefs.h>

#include "fusion_types.h"
#include "property.h"

#include "fusion_internal.h"


#ifndef FUSION_FAKE

#define SEMOP(s,o,n)                              \
     while (semop (s, o, n)) {                    \
          FPERROR ("semop");                      \
                                                  \
          switch (errno) {                        \
               case EINTR:                        \
                    continue;                     \
               case EACCES:                       \
                    return FUSION_ACCESSDENIED;   \
               case EIDRM:                        \
                    return FUSION_DESTROYED;      \
          }                                       \
                                                  \
          return FUSION_FAILURE;                  \
     }

/*
 * Initializes the property
 */
FusionResult
fusion_property_init (FusionProperty *property)
{
     union semun semopts;

     /* create three semaphores, each one for locking, counting, waiting */
     property->sem_id = semget (IPC_PRIVATE, 3, IPC_CREAT | 0660);
     if (property->sem_id < 0) {
          FPERROR ("semget");

          if (errno == ENOMEM || errno == ENOSPC)
               return FUSION_LIMITREACHED;

          return FUSION_FAILURE;
     }

     /* initialize the lock */
     semopts.val = 1;
     if (semctl (property->sem_id, 0, SETVAL, semopts)) {
          FPERROR ("semctl");

          semctl (property->sem_id, 0, IPC_RMID, 0);
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

/*
 * Lease the property causing others to wait before leasing or purchasing.
 */
FusionResult
fusion_property_lease (FusionProperty *property)
{
     FusionResult  ret = FUSION_SUCCESS;
     struct sembuf op[3];

     /* lock */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->sem_id, op, 1);

     while (property->state == FUSION_PROPERTY_LEASED) {
          /* increment wait counter */
          op[0].sem_num = 1;
          op[0].sem_op  = 1;
          op[0].sem_flg = SEM_UNDO;
          
          /* unlock */
          op[1].sem_num = 0;
          op[1].sem_op  = 1;
          op[1].sem_flg = SEM_UNDO;

          SEMOP (property->sem_id, op, 2);
          
          /* lock */
          op[0].sem_num = 0;
          op[0].sem_op  = -1;
          op[0].sem_flg = SEM_UNDO;
          
          /* wait */
          op[1].sem_num = 2;
          op[1].sem_op  = -1;
          op[1].sem_flg = 0;
          
          /* decrement wait counter */
          op[2].sem_num = 1;
          op[2].sem_op  = -1;
          op[2].sem_flg = SEM_UNDO;
          
          SEMOP (property->sem_id, op, 3);
     }

     if (property->state == FUSION_PROPERTY_AVAILABLE)
          property->state = FUSION_PROPERTY_LEASED;
     else
          ret = FUSION_INUSE;

     /* unlock */
     op[0].sem_num = 0;
     op[0].sem_op  = 1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->sem_id, op, 1);

     return ret;
}

/*
 * Purchase the property disallowing others to lease or purchase it.
 */
FusionResult
fusion_property_purchase (FusionProperty *property)
{
     struct sembuf op[3];

     /* lock */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->sem_id, op, 1);

     while (property->state == FUSION_PROPERTY_LEASED) {
          /* increment wait counter */
          op[0].sem_num = 1;
          op[0].sem_op  = 1;
          op[0].sem_flg = SEM_UNDO;
          
          /* unlock */
          op[1].sem_num = 0;
          op[1].sem_op  = 1;
          op[1].sem_flg = SEM_UNDO;

          SEMOP (property->sem_id, op, 2);
          
          /* lock */
          op[0].sem_num = 0;
          op[0].sem_op  = -1;
          op[0].sem_flg = SEM_UNDO;
          
          /* wait */
          op[1].sem_num = 2;
          op[1].sem_op  = -1;
          op[1].sem_flg = 0;
          
          /* decrement wait counter */
          op[2].sem_num = 1;
          op[2].sem_op  = -1;
          op[2].sem_flg = SEM_UNDO;
          
          SEMOP (property->sem_id, op, 3);
     }

     if (property->state == FUSION_PROPERTY_AVAILABLE) {
          union semun semopts;
          int         waiters;

          property->state = FUSION_PROPERTY_PURCHASED;
     
          /* get number of waiters */
          waiters = semctl (property->sem_id, 1, GETVAL, semopts);
          if (waiters < 0) {
               FPERROR ("semctl");

               /* unlock */
               op[0].sem_num = 0;
               op[0].sem_op  = 1;
               op[0].sem_flg = SEM_UNDO;

               SEMOP (property->sem_id, op, 1);
               
               return FUSION_FAILURE;
          }

          /* unlock */
          op[0].sem_num = 0;
          op[0].sem_op  = 1;
          op[0].sem_flg = SEM_UNDO;

          /* awake waiters */
          op[1].sem_num = 2;
          op[1].sem_op  = waiters;
          op[1].sem_flg = 0;

          SEMOP (property->sem_id, op, waiters ? 2 : 1);

          return FUSION_SUCCESS;
     }
     
     /* unlock */
     op[0].sem_num = 0;
     op[0].sem_op  = 1;
     op[0].sem_flg = SEM_UNDO;
     
     SEMOP (property->sem_id, op, 1);

     return FUSION_INUSE;
}

/*
 * Cede the property allowing others to lease or purchase it.
 */
FusionResult
fusion_property_cede (FusionProperty *property)
{
     struct sembuf op[2];
     union semun   semopts;
     int           waiters;

     /* lock */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->sem_id, op, 1);

     /* debug check */
     if (property->state == FUSION_PROPERTY_AVAILABLE)
          FDEBUG("BUG! property not leased/purchased!");
          
     /* make available */
     property->state = FUSION_PROPERTY_AVAILABLE;

     /* get number of waiters */
     waiters = semctl (property->sem_id, 1, GETVAL, semopts);
     if (waiters < 0) {
          FPERROR ("semctl");

          /* unlock */
          op[0].sem_num = 0;
          op[0].sem_op  = 1;
          op[0].sem_flg = SEM_UNDO;

          SEMOP (property->sem_id, op, 1);
          
          return FUSION_FAILURE;
     }

     /* unlock */
     op[0].sem_num = 0;
     op[0].sem_op  = 1;
     op[0].sem_flg = SEM_UNDO;

     /* awake waiters */
     op[1].sem_num = 2;
     op[1].sem_op  = waiters;
     op[1].sem_flg = 0;

     SEMOP (property->sem_id, op, waiters ? 2 : 1);

     return FUSION_SUCCESS;
}

/*
 * Destroys the property
 */
FusionResult
fusion_property_destroy (FusionProperty *property)
{
     union semun semopts;

     if (semctl (property->sem_id, 0, IPC_RMID, semopts)) {
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

#else

#include <pthread.h>

/*
 * Initializes the property
 */
FusionResult
fusion_property_init (FusionProperty *property)
{
     pthread_mutex_init (&property->lock, NULL);
     pthread_cond_init (&property->cond, NULL);

     property->state = FUSION_PROPERTY_AVAILABLE;

     return FUSION_SUCCESS;
}

/*
 * Lease the property causing others to wait before leasing or purchasing.
 */
FusionResult
fusion_property_lease (FusionProperty *property)
{
     FusionResult ret = FUSION_SUCCESS;

     pthread_mutex_lock (&property->lock);

     /* Wait as long as the property is leased by another party. */
     while (property->state == FUSION_PROPERTY_LEASED)
          pthread_cond_wait (&property->cond, &property->lock);

     /* Fail if purchased by another party, otherwise succeed. */
     if (property->state == FUSION_PROPERTY_PURCHASED)
          ret = FUSION_INUSE;
     else
          property->state = FUSION_PROPERTY_LEASED;

     pthread_mutex_unlock (&property->lock);

     return ret;
}

/*
 * Purchase the property disallowing others to lease or purchase it.
 */
FusionResult
fusion_property_purchase (FusionProperty *property)
{
     FusionResult ret = FUSION_SUCCESS;

     pthread_mutex_lock (&property->lock);

     /* Wait as long as the property is leased by another party. */
     while (property->state == FUSION_PROPERTY_LEASED)
          pthread_cond_wait (&property->cond, &property->lock);

     /* Fail if purchased by another party, otherwise succeed. */
     if (property->state == FUSION_PROPERTY_PURCHASED)
          ret = FUSION_INUSE;
     else {
          property->state = FUSION_PROPERTY_PURCHASED;

          /* Wake up any other waiting party. */
          pthread_cond_broadcast (&property->cond);
     }

     pthread_mutex_unlock (&property->lock);

     return ret;
}

/*
 * Cede the property allowing others to lease or purchase it.
 */
FusionResult
fusion_property_cede (FusionProperty *property)
{
     pthread_mutex_lock (&property->lock);

     /* Simple error checking, maybe we should also check the owner. */
     DFB_ASSERT( property->state != FUSION_PROPERTY_AVAILABLE );

     /* Put back into 'available' state. */
     property->state = FUSION_PROPERTY_AVAILABLE;

     /* Wake up one waiting party if there are any. */
     pthread_cond_signal (&property->cond);
     
     pthread_mutex_unlock (&property->lock);
     
     return FUSION_SUCCESS;
}

/*
 * Destroys the property
 */
FusionResult
fusion_property_destroy (FusionProperty *property)
{
     pthread_cond_destroy (&property->cond);
     pthread_mutex_destroy (&property->lock);
     
     return FUSION_SUCCESS;
}

#endif /* !FUSION_FAKE */

