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

#include <fusion/build.h>

#if FUSION_BUILD_MULTI
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/types.h>
#include <fusion/ref.h>

#include "fusion_internal.h"

#include <signal.h>


#if FUSION_BUILD_MULTI

FusionResult
fusion_ref_init (FusionRef *ref)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, FUSION_REF_NEW, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_NEW");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_up (FusionRef *ref, bool global)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, global ?
                   FUSION_REF_UP_GLOBAL : FUSION_REF_UP, &ref->id))
     {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          if (global)
               D_PERROR ("FUSION_REF_UP_GLOBAL");
          else
               D_PERROR ("FUSION_REF_UP");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_down (FusionRef *ref, bool global)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, global ?
                   FUSION_REF_DOWN_GLOBAL : FUSION_REF_DOWN, &ref->id))
     {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          if (global)
               D_PERROR ("FUSION_REF_DOWN_GLOBAL");
          else
               D_PERROR ("FUSION_REF_DOWN");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_stat (FusionRef *ref, int *refs)
{
     int val;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );
     D_ASSERT( refs != NULL );

     while ((val = ioctl (_fusion_fd, FUSION_REF_STAT, &ref->id)) < 0) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_STAT");

          return FUSION_FAILURE;
     }

     *refs = val;

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_zero_lock (FusionRef *ref)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, FUSION_REF_ZERO_LOCK, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_ZERO_LOCK");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, FUSION_REF_ZERO_TRYLOCK, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETOOMANYREFS:
                    return FUSION_INUSE;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_ZERO_TRYLOCK");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_unlock (FusionRef *ref)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, FUSION_REF_UNLOCK, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_UNLOCK");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_watch (FusionRef *ref, FusionCall *call, int call_arg)
{
     FusionRefWatch watch;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );
     D_ASSERT( call != NULL );

     watch.id       = ref->id;
     watch.call_id  = call->call_id;
     watch.call_arg = call_arg;

     while (ioctl (_fusion_fd, FUSION_REF_WATCH, &watch)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_WATCH");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_destroy (FusionRef *ref)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, FUSION_REF_DESTROY, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_DESTROY");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

#else /* FUSION_BUILD_MULTI */

FusionResult
fusion_ref_init (FusionRef *ref)
{
     D_ASSERT( ref != NULL );

     direct_util_recursive_pthread_mutex_init (&ref->fake.lock);
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

     D_ASSERT( ref != NULL );

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
     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (!ref->fake.refs) {
          D_BUG( "no more references" );
          pthread_mutex_unlock (&ref->fake.lock);
          return FUSION_BUG;
     }

     if (ref->fake.destroyed) {
          pthread_mutex_unlock (&ref->fake.lock);
          return FUSION_DESTROYED;
     }

     if (! --ref->fake.refs && ref->fake.call) {
          FusionCall *call = ref->fake.call;

          if (call->handler)
               call->handler( 0, ref->fake.call_arg, NULL, call->ctx );
     }

     pthread_mutex_unlock (&ref->fake.lock);

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_stat (FusionRef *ref, int *refs)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( refs != NULL );

     if (ref->fake.destroyed)
          return FUSION_DESTROYED;

     *refs = ref->fake.refs;

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_zero_lock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = FUSION_DESTROYED;
     else if (ref->fake.call)
          ret = FUSION_ACCESSDENIED;
     else while (ref->fake.refs && !ret) {
          ref->fake.waiting++;
          pthread_cond_wait (&ref->fake.cond, &ref->fake.lock);
          ref->fake.waiting--;

          if (ref->fake.destroyed)
               ret = FUSION_DESTROYED;
          else if (ref->fake.call)
               ret = FUSION_ACCESSDENIED;
     }

     if (ret != FUSION_SUCCESS)
          pthread_mutex_unlock (&ref->fake.lock);

     return ret;
}

FusionResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     FusionResult ret = FUSION_SUCCESS;

     D_ASSERT( ref != NULL );

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
     D_ASSERT( ref != NULL );

     pthread_mutex_unlock (&ref->fake.lock);

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_watch (FusionRef *ref, FusionCall *call, int call_arg)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( call != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed) {
          pthread_mutex_unlock (&ref->fake.lock);
          return FUSION_DESTROYED;
     }

     if (!ref->fake.refs) {
          pthread_mutex_unlock (&ref->fake.lock);
          return FUSION_BUG;
     }

     if (ref->fake.call) {
          pthread_mutex_unlock (&ref->fake.lock);
          return FUSION_INUSE;
     }

     ref->fake.call     = call;
     ref->fake.call_arg = call_arg;

     pthread_mutex_unlock (&ref->fake.lock);

     return FUSION_SUCCESS;
}

FusionResult
fusion_ref_destroy (FusionRef *ref)
{
     D_ASSERT( ref != NULL );

     ref->fake.destroyed = true;

     if (ref->fake.waiting)
          pthread_cond_broadcast (&ref->fake.cond);

     pthread_mutex_unlock (&ref->fake.lock);
     pthread_cond_destroy (&ref->fake.cond);

     return FUSION_SUCCESS;
}

#endif

