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

#ifndef __PROPERTY_H__
#define __PROPERTY_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "fusion_types.h"

typedef enum {
     FUSION_PROPERTY_AVAILABLE,
     FUSION_PROPERTY_LEASED,
     FUSION_PROPERTY_PURCHASED
} FusionPropertyState;


#ifndef FUSION_FAKE
     typedef struct {
          int                 sem_id;
          FusionPropertyState state;
     } FusionProperty;

#else

#include <pthread.h>
     
     typedef struct {
          pthread_mutex_t     lock;
          pthread_cond_t      cond;
          FusionPropertyState state;
     } FusionProperty;

#endif

/*
 * Initializes the property
 */
FusionResult fusion_property_init     (FusionProperty *property);

/*
 * Lease the property causing others to wait before leasing or purchasing.
 *
 * Waits as long as property is leased by another party.
 * Returns FUSION_INUSE if property is/gets purchased by another party.
 *
 * Succeeds if property is available,
 * puts the property into 'leased' state.
 */
FusionResult fusion_property_lease    (FusionProperty *property);

/*
 * Purchase the property disallowing others to lease or purchase it.
 *
 * Waits as long as property is leased by another party.
 * Returns FUSION_INUSE if property is/gets purchased by another party.
 *
 * Succeeds if property is available,
 * puts the property into 'purchased' state and wakes up any waiting party.
 */
FusionResult fusion_property_purchase (FusionProperty *property);

/*
 * Cede the property allowing others to lease or purchase it.
 *
 * Puts the property into 'available' state and wakes up one waiting party.
 */
FusionResult fusion_property_cede     (FusionProperty *property);

/*
 * Destroys the property
 */
FusionResult fusion_property_destroy  (FusionProperty *property);

#ifdef __cplusplus
}
#endif

#endif /* __PROPERTY_H__ */

