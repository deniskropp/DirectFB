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

#ifndef FUSION_FAKE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>

#if LINUX_FUSION
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#endif

#include <core/coredefs.h>

#include "fusion_types.h"
#include "property.h"

#include "fusion_internal.h"


#ifndef FUSION_FAKE

#if LINUX_FUSION

FusionResult
fusion_property_init (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );
     
     if (ioctl (_fusion_fd(), FUSION_PROPERTY_NEW, &property->id)) {
          FPERROR ("FUSION_PROPERTY_NEW");
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_property_lease (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );
     
     while (ioctl (_fusion_fd(), FUSION_PROPERTY_LEASE, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EINVAL:
                    FERROR ("property already destroyed\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("FUSION_PROPERTY_LEASE");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_property_purchase (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );
     
     while (ioctl (_fusion_fd(), FUSION_PROPERTY_PURCHASE, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EINVAL:
                    FERROR ("property already destroyed\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_PROPERTY_PURCHASE");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_property_cede (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );
     
     while (ioctl (_fusion_fd(), FUSION_PROPERTY_CEDE, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("property already destroyed\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_PROPERTY_CEDE");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_property_destroy (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );
     
     while (ioctl (_fusion_fd(), FUSION_PROPERTY_DESTROY, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("property already destroyed\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_PROPERTY_DESTROY");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

#else

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
     
static void check_orphaned_property (FusionProperty *property);
static FusionResult wait_until_not_leased (FusionProperty *property);
static FusionResult awake_all_waiters (FusionProperty *property);

/*
 * Initializes the property
 */
FusionResult
fusion_property_init (FusionProperty *property)
{
     union semun semopts;

     /* create four semaphores: locking, counting, waiting, orphan detection */
     property->id = semget (IPC_PRIVATE, 4, IPC_CREAT | 0660);
     if (property->id < 0) {
          FPERROR ("semget");

          if (errno == ENOMEM || errno == ENOSPC)
               return FUSION_LIMITREACHED;

          return FUSION_FAILURE;
     }

     /* initialize the lock */
     semopts.val = 1;
     if (semctl (property->id, 0, SETVAL, semopts)) {
          FPERROR ("semctl");

          semctl (property->id, 0, IPC_RMID, 0);
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
     struct sembuf op[1];

     /* lock */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->id, op, 1);

     /* detect orphan */
     check_orphaned_property (property);

     ret = wait_until_not_leased (property);
     if (ret == FUSION_SUCCESS) {
          if (property->state == FUSION_PROPERTY_AVAILABLE) {
               property->state = FUSION_PROPERTY_LEASED;

               /* orphan detection */
               op[0].sem_num = 3;
               op[0].sem_op  = 1;
               op[0].sem_flg = SEM_UNDO;

               SEMOP (property->id, op, 1);
          }
          else
               ret = FUSION_INUSE;
     }

     /* unlock */
     op[0].sem_num = 0;
     op[0].sem_op  = 1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->id, op, 1);

     return ret;
}

/*
 * Purchase the property disallowing others to lease or purchase it.
 */
FusionResult
fusion_property_purchase (FusionProperty *property)
{
     FusionResult  ret;
     struct sembuf op[1];

     /* lock */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->id, op, 1);

     /* detect orphan */
     check_orphaned_property (property);

     ret = wait_until_not_leased (property);
     if (ret == FUSION_SUCCESS) {
          if (property->state == FUSION_PROPERTY_AVAILABLE) {
               property->state = FUSION_PROPERTY_PURCHASED;

               /* orphan detection */
               op[0].sem_num = 3;
               op[0].sem_op  = 1;
               op[0].sem_flg = SEM_UNDO;

               SEMOP (property->id, op, 1);

               awake_all_waiters (property);
          }
          else
               ret = FUSION_INUSE;
     }
     
     /* unlock */
     op[0].sem_num = 0;
     op[0].sem_op  = 1;
     op[0].sem_flg = SEM_UNDO;
     
     SEMOP (property->id, op, 1);

     return ret;
}

/*
 * Cede the property allowing others to lease or purchase it.
 */
FusionResult
fusion_property_cede (FusionProperty *property)
{
     struct sembuf op[2];

     /* lock */
     op[0].sem_num = 0;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;

     SEMOP (property->id, op, 1);

     /* debug check */
     if (property->state == FUSION_PROPERTY_AVAILABLE)
          FDEBUG("BUG! property not leased/purchased!\n");
          
     /* make available */
     property->state = FUSION_PROPERTY_AVAILABLE;

     /* orphan detection */
     op[0].sem_num = 3;
     op[0].sem_op  = -1;
     op[0].sem_flg = SEM_UNDO;

     /* unlock */
     op[1].sem_num = 0;
     op[1].sem_op  = 1;
     op[1].sem_flg = SEM_UNDO;

     SEMOP (property->id, op, 2);

     awake_all_waiters (property);
     
     return FUSION_SUCCESS;
}

/*
 * Destroys the property
 */
FusionResult
fusion_property_destroy (FusionProperty *property)
{
     union semun semopts;

     if (semctl (property->id, 0, IPC_RMID, semopts)) {
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

static void
check_orphaned_property (FusionProperty *property)
{
     union semun semopts;

     if (property->state == FUSION_PROPERTY_AVAILABLE)
          return;
     
     switch (semctl (property->id, 3, GETVAL, semopts)) {
          case 1:
               return;
          
          case 0:
               FDEBUG ("orphaned property detected!\n");

               property->state = FUSION_PROPERTY_AVAILABLE;

               awake_all_waiters (property);
               break;

          default:
               FPERROR ("semctl");
               break;
     }
}

static FusionResult
wait_until_not_leased (FusionProperty *property)
{
     struct sembuf op[3];
     
     while (property->state == FUSION_PROPERTY_LEASED) {
          /* increment wait counter */
          op[0].sem_num = 1;
          op[0].sem_op  = 1;
          op[0].sem_flg = SEM_UNDO;
          
          /* unlock */
          op[1].sem_num = 0;
          op[1].sem_op  = 1;
          op[1].sem_flg = SEM_UNDO;

          SEMOP (property->id, op, 2);
          
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
          
          SEMOP (property->id, op, 3);
     }

     return FUSION_SUCCESS;
}

static FusionResult
awake_all_waiters (FusionProperty *property)
{
     union semun   semopts;
     int           waiters;
     struct sembuf op[1];

     /* get number of waiters */
     waiters = semctl (property->id, 1, GETVAL, semopts);
     if (waiters < 0) {
          FPERROR ("semctl");
          return FUSION_FAILURE;
     }

     if (!waiters)
          return FUSION_SUCCESS;

     /* awake waiters */
     op[0].sem_num = 2;
     op[0].sem_op  = waiters;
     op[0].sem_flg = 0;

     SEMOP (property->id, op, 1);

     return FUSION_SUCCESS;
}

#endif

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

