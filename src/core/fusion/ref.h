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

#ifndef __REF_H__
#define __REF_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "fusion_types.h"


#ifndef FUSION_FAKE

     typedef struct {
          int sem_id;
     } FusionRef;

#else

     #include <pthread.h>
     
     typedef struct {
          int             refs;
          pthread_cond_t  cond;
          pthread_mutex_t lock;
          bool            destroyed;
     } FusionRef;

#endif

/*
 * Initialize.
 */
FusionResult fusion_ref_init         (FusionRef *ref);

/*
 * Lock, increase, unlock.
 */
FusionResult fusion_ref_up           (FusionRef *ref, bool global);

/*
 * Lock, decrease, unlock.
 */
FusionResult fusion_ref_down         (FusionRef *ref, bool global);

/*
 * Wait for zero and lock.
 */
FusionResult fusion_ref_zero_lock    (FusionRef *ref);

/*
 * Check for zero and lock if true.
 */
FusionResult fusion_ref_zero_trylock (FusionRef *ref);

/*
 * Unlock the counter.
 * Only to be called after successful zero_lock or zero_trylock.
 */
FusionResult fusion_ref_unlock       (FusionRef *ref);

/*
 * Deinitialize.
 * Can be called after successful zero_lock or zero_trylock
 * so that waiting fusion_ref_up calls return with FUSION_DESTROYED.
 */
FusionResult fusion_ref_destroy      (FusionRef *ref);

#ifdef __cplusplus
}
#endif

#endif /* __REF_H__ */

