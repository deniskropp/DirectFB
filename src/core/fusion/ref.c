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

FusionResult
fusion_ref_init (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     while (ioctl (fusion_fd, FUSION_REF_NEW, &ref->ref_id)) {
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

     while (ioctl (fusion_fd, global ?
                   FUSION_REF_UP_GLOBAL : FUSION_REF_UP, &ref->ref_id))
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
     
     while (ioctl (fusion_fd, global ?
                   FUSION_REF_DOWN_GLOBAL : FUSION_REF_DOWN, &ref->ref_id))
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

     while ((val = ioctl (fusion_fd, FUSION_REF_STAT, &ref->ref_id)) < 0) {
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
     
     while (ioctl (fusion_fd, FUSION_REF_ZERO_LOCK, &ref->ref_id)) {
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
     
     while (ioctl (fusion_fd, FUSION_REF_ZERO_TRYLOCK, &ref->ref_id)) {
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
     
     while (ioctl (fusion_fd, FUSION_REF_UNLOCK, &ref->ref_id)) {
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

     while (ioctl (fusion_fd, FUSION_REF_DESTROY, &ref->ref_id)) {
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
     
     pthread_mutex_init (&ref->lock, NULL);
     pthread_cond_init (&ref->cond, NULL);

     ref->refs      = 0;
     ref->destroyed = false;
     ref->waiting   = 0;

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_up (FusionRef *ref, bool global)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else
          ref->refs++;
     
     pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult
fusion_ref_down (FusionRef *ref, bool global)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else
          ref->refs--;
     
     if (ref->waiting)
          pthread_cond_broadcast (&ref->cond);
     
     pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult
fusion_ref_stat (FusionRef *ref, int *refs)
{
     DFB_ASSERT( ref != NULL );
     DFB_ASSERT( refs != NULL );

     if (ref->destroyed)
          return FUSION_DESTROYED;

     *refs = ref->refs;

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_zero_lock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else while (ref->refs && !ret) {
          ref->waiting++;
          pthread_cond_wait (&ref->cond, &ref->lock);
          ref->waiting--;
          
          if (ref->destroyed)
               ret = FUSION_DESTROYED;
     }
     
     if (ret != FUSION_SUCCESS)
          pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_lock (&ref->lock);

     if (ref->destroyed)
          ret = FUSION_DESTROYED;
     else if (ref->refs)
          ret = FUSION_INUSE;
     
     if (ret != FUSION_SUCCESS)
          pthread_mutex_unlock (&ref->lock);
     
     return ret;
}

FusionResult
fusion_ref_unlock (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     pthread_mutex_unlock (&ref->lock);

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_destroy (FusionRef *ref)
{
     DFB_ASSERT( ref != NULL );
     
     ref->destroyed = true;

     if (ref->waiting)
          pthread_cond_broadcast (&ref->cond);

     pthread_mutex_unlock (&ref->lock);
     pthread_cond_destroy (&ref->cond);
     
     return FUSION_SUCCESS;
}

#endif

/*******************************
 *  Fusion internal functions  *
 *******************************/



/*****************************
 *  File internal functions  *
 *****************************/

