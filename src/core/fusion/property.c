/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#include <core/coredefs.h>

#include "fusion_types.h"
#include "property.h"

#include "fusion_internal.h"


#ifndef FUSION_FAKE

FusionResult
fusion_property_init (FusionProperty *property)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( property != NULL );

     while (ioctl (_fusion_fd, FUSION_PROPERTY_NEW, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          FPERROR ("FUSION_PROPERTY_NEW");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_property_lease (FusionProperty *property)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( property != NULL );

     while (ioctl (_fusion_fd, FUSION_PROPERTY_LEASE, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EINVAL:
                    FERROR ("invalid property\n");
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
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( property != NULL );

     while (ioctl (_fusion_fd, FUSION_PROPERTY_PURCHASE, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EINVAL:
                    FERROR ("invalid property\n");
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
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( property != NULL );

     while (ioctl (_fusion_fd, FUSION_PROPERTY_CEDE, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid property\n");
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
fusion_property_holdup (FusionProperty *property)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( property != NULL );

     while (ioctl (_fusion_fd, FUSION_PROPERTY_HOLDUP, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid property\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("FUSION_PROPERTY_HOLDUP");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_property_destroy (FusionProperty *property)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( property != NULL );

     while (ioctl (_fusion_fd, FUSION_PROPERTY_DESTROY, &property->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid property\n");
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

#include <pthread.h>

/*
 * Initializes the property
 */
FusionResult
fusion_property_init (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );

     fusion_pthread_recursive_mutex_init (&property->fake.lock);
     pthread_cond_init (&property->fake.cond, NULL);

     property->fake.state = FUSION_PROPERTY_AVAILABLE;

     return FUSION_SUCCESS;
}

/*
 * Lease the property causing others to wait before leasing or purchasing.
 */
FusionResult
fusion_property_lease (FusionProperty *property)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( property != NULL );

     pthread_mutex_lock (&property->fake.lock);

     /* Wait as long as the property is leased by another party. */
     while (property->fake.state == FUSION_PROPERTY_LEASED)
          pthread_cond_wait (&property->fake.cond, &property->fake.lock);

     /* Fail if purchased by another party, otherwise succeed. */
     if (property->fake.state == FUSION_PROPERTY_PURCHASED)
          ret = FUSION_INUSE;
     else
          property->fake.state = FUSION_PROPERTY_LEASED;

     pthread_mutex_unlock (&property->fake.lock);

     return ret;
}

/*
 * Purchase the property disallowing others to lease or purchase it.
 */
FusionResult
fusion_property_purchase (FusionProperty *property)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( property != NULL );

     pthread_mutex_lock (&property->fake.lock);

     /* Wait as long as the property is leased by another party. */
     while (property->fake.state == FUSION_PROPERTY_LEASED)
          pthread_cond_wait (&property->fake.cond, &property->fake.lock);

     /* Fail if purchased by another party, otherwise succeed. */
     if (property->fake.state == FUSION_PROPERTY_PURCHASED)
          ret = FUSION_INUSE;
     else {
          property->fake.state = FUSION_PROPERTY_PURCHASED;

          /* Wake up any other waiting party. */
          pthread_cond_broadcast (&property->fake.cond);
     }

     pthread_mutex_unlock (&property->fake.lock);

     return ret;
}

/*
 * Cede the property allowing others to lease or purchase it.
 */
FusionResult
fusion_property_cede (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );

     pthread_mutex_lock (&property->fake.lock);

     /* Simple error checking, maybe we should also check the owner. */
     DFB_ASSERT( property->fake.state != FUSION_PROPERTY_AVAILABLE );

     /* Put back into 'available' state. */
     property->fake.state = FUSION_PROPERTY_AVAILABLE;

     /* Wake up one waiting party if there are any. */
     pthread_cond_signal (&property->fake.cond);

     pthread_mutex_unlock (&property->fake.lock);

     return FUSION_SUCCESS;
}

/*
 * Does nothing to avoid killing ourself.
 */
FusionResult
fusion_property_holdup (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );

     return FUSION_SUCCESS;
}

/*
 * Destroys the property
 */
FusionResult
fusion_property_destroy (FusionProperty *property)
{
     DFB_ASSERT( property != NULL );

     pthread_cond_destroy (&property->fake.cond);
     pthread_mutex_destroy (&property->fake.lock);

     return FUSION_SUCCESS;
}

#endif /* !FUSION_FAKE */

