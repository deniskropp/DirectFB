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

#ifndef FUSION_FAKE
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#include "fusion_types.h"
#include "ref.h"

#include "fusion_internal.h"

#include <signal.h>
 

#ifndef FUSION_FAKE

FusionResult
fusion_ref_init (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     while (ioctl (_fusion_fd, FUSION_REF_NEW, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REF_NEW");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_up (FusionRef *ref, bool global)
{
     DFB_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, global ?
                   FUSION_REF_UP_GLOBAL : FUSION_REF_UP, &ref->id))
     {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          if (global)
               FPERROR ("FUSION_REF_UP_GLOBAL");
          else
               FPERROR ("FUSION_REF_UP");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_down (FusionRef *ref, bool global)
{
     DFB_ASSERT( ref != NULL );
     
     while (ioctl (_fusion_fd, global ?
                   FUSION_REF_DOWN_GLOBAL : FUSION_REF_DOWN, &ref->id))
     {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          if (global)
               FPERROR ("FUSION_REF_DOWN_GLOBAL");
          else
               FPERROR ("FUSION_REF_DOWN");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_stat (FusionRef *ref, int *refs)
{
     int val;

     DFB_ASSERT( ref != NULL );
     DFB_ASSERT( refs != NULL );

     while ((val = ioctl (_fusion_fd, FUSION_REF_STAT, &ref->id)) < 0) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REF_STAT");
          
          return FUSION_FAILURE;
     }

     *refs = val;

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_zero_lock (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     while (ioctl (_fusion_fd, FUSION_REF_ZERO_LOCK, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REF_ZERO_LOCK");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     while (ioctl (_fusion_fd, FUSION_REF_ZERO_TRYLOCK, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETOOMANYREFS:
                    return FUSION_INUSE;
               case EINVAL:
                    FERROR ("invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REF_ZERO_TRYLOCK");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_unlock (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     while (ioctl (_fusion_fd, FUSION_REF_UNLOCK, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REF_UNLOCK");
          
          return FUSION_FAILURE;
     }
     
     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_destroy (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, FUSION_REF_DESTROY, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REF_DESTROY");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

#else /* !FUSION_FAKE */

FusionResult
fusion_ref_init (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_init (&ref->fake.lock, NULL);
     pthread_cond_init (&ref->fake.cond, NULL);

     ref->fake.refs      = 0;
     ref->fake.destroyed = false;
     ref->fake.waiting   = 0;

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_up (FusionRef *ref, bool global)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = FUSION_DESTROYED;
     else
          ref->fake.refs++;
     
     pthread_mutex_unlock (&ref->fake.lock);
     
     return ret;
}

FusionResult
fusion_ref_down (FusionRef *ref, bool global)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = FUSION_DESTROYED;
     else
          ref->fake.refs--;
     
     if (ref->fake.waiting)
          pthread_cond_broadcast (&ref->fake.cond);
     
     pthread_mutex_unlock (&ref->fake.lock);
     
     return ret;
}

FusionResult
fusion_ref_stat (FusionRef *ref, int *refs)
{
     DFB_ASSERT( ref != NULL );
     DFB_ASSERT( refs != NULL );

     if (ref->fake.destroyed)
          return FUSION_DESTROYED;

     *refs = ref->fake.refs;

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_zero_lock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = FUSION_DESTROYED;
     else while (ref->fake.refs && !ret) {
          ref->fake.waiting++;
          pthread_cond_wait (&ref->fake.cond, &ref->fake.lock);
          ref->fake.waiting--;
          
          if (ref->fake.destroyed)
               ret = FUSION_DESTROYED;
     }
     
     if (ret != FUSION_SUCCESS)
          pthread_mutex_unlock (&ref->fake.lock);
     
     return ret;
}

FusionResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = FUSION_DESTROYED;
     else if (ref->fake.refs)
          ret = FUSION_INUSE;
     
     if (ret != FUSION_SUCCESS)
          pthread_mutex_unlock (&ref->fake.lock);
     
     return ret;
}

FusionResult
fusion_ref_unlock (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_unlock (&ref->fake.lock);

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_destroy (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     ref->fake.destroyed = true;

     if (ref->fake.waiting)
          pthread_cond_broadcast (&ref->fake.cond);

     pthread_mutex_unlock (&ref->fake.lock);
     pthread_cond_destroy (&ref->fake.cond);
     
     return FUSION_SUCCESS;
}

#endif

