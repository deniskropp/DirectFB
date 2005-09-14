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

DirectResult
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

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
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
                    return DFB_DESTROYED;
               default:
                    break;
          }

          if (global)
               D_PERROR ("FUSION_REF_UP_GLOBAL");
          else
               D_PERROR ("FUSION_REF_UP");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
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
                    return DFB_DESTROYED;
               default:
                    break;
          }

          if (global)
               D_PERROR ("FUSION_REF_DOWN_GLOBAL");
          else
               D_PERROR ("FUSION_REF_DOWN");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
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
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_STAT");

          return DFB_FAILURE;
     }

     *refs = val;

     return DFB_OK;
}

DirectResult
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
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_ZERO_LOCK");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd, FUSION_REF_ZERO_TRYLOCK, &ref->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETOOMANYREFS:
                    return DFB_BUSY;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_ZERO_TRYLOCK");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
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
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_UNLOCK");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
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
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_WATCH");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
fusion_ref_inherit (FusionRef *ref, FusionRef *from)
{
     FusionRefInherit inherit;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( ref != NULL );
     D_ASSERT( from != NULL );

     inherit.id   = ref->id;
     inherit.from = from->id;

     while (ioctl (_fusion_fd, FUSION_REF_INHERIT, &inherit)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reference: invalid reference\n");
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_INHERIT");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

DirectResult
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
                    return DFB_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REF_DESTROY");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

#else /* FUSION_BUILD_MULTI */

DirectResult
fusion_ref_init (FusionRef *ref)
{
     D_ASSERT( ref != NULL );

     direct_util_recursive_pthread_mutex_init (&ref->fake.lock);
     pthread_cond_init (&ref->fake.cond, NULL);

     ref->fake.refs      = 0;
     ref->fake.destroyed = false;
     ref->fake.waiting   = 0;

     return DFB_OK;
}

DirectResult
fusion_ref_up (FusionRef *ref, bool global)
{
     DirectResult ret = DFB_OK;

     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = DFB_DESTROYED;
     else
          ref->fake.refs++;

     pthread_mutex_unlock (&ref->fake.lock);

     return ret;
}

DirectResult
fusion_ref_down (FusionRef *ref, bool global)
{
     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (!ref->fake.refs) {
          D_BUG( "no more references" );
          pthread_mutex_unlock (&ref->fake.lock);
          return DFB_BUG;
     }

     if (ref->fake.destroyed) {
          pthread_mutex_unlock (&ref->fake.lock);
          return DFB_DESTROYED;
     }

     if (! --ref->fake.refs && ref->fake.call) {
          FusionCall *call = ref->fake.call;

          if (call->handler) {
               pthread_mutex_unlock (&ref->fake.lock);
               call->handler( 0, ref->fake.call_arg, NULL, call->ctx );
               return DFB_OK;
          }
     }

     pthread_mutex_unlock (&ref->fake.lock);

     return DFB_OK;
}

DirectResult
fusion_ref_stat (FusionRef *ref, int *refs)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( refs != NULL );

     if (ref->fake.destroyed)
          return DFB_DESTROYED;

     *refs = ref->fake.refs;

     return DFB_OK;
}

DirectResult
fusion_ref_zero_lock (FusionRef *ref)
{
     DirectResult ret = DFB_OK;

     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = DFB_DESTROYED;
     else if (ref->fake.call)
          ret = DFB_ACCESSDENIED;
     else while (ref->fake.refs && !ret) {
          ref->fake.waiting++;
          pthread_cond_wait (&ref->fake.cond, &ref->fake.lock);
          ref->fake.waiting--;

          if (ref->fake.destroyed)
               ret = DFB_DESTROYED;
          else if (ref->fake.call)
               ret = DFB_ACCESSDENIED;
     }

     if (ret != DFB_OK)
          pthread_mutex_unlock (&ref->fake.lock);

     return ret;
}

DirectResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     DirectResult ret = DFB_OK;

     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed)
          ret = DFB_DESTROYED;
     else if (ref->fake.refs)
          ret = DFB_BUSY;

     if (ret != DFB_OK)
          pthread_mutex_unlock (&ref->fake.lock);

     return ret;
}

DirectResult
fusion_ref_unlock (FusionRef *ref)
{
     D_ASSERT( ref != NULL );

     pthread_mutex_unlock (&ref->fake.lock);

     return DFB_OK;
}

DirectResult
fusion_ref_watch (FusionRef *ref, FusionCall *call, int call_arg)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( call != NULL );

     pthread_mutex_lock (&ref->fake.lock);

     if (ref->fake.destroyed) {
          pthread_mutex_unlock (&ref->fake.lock);
          return DFB_DESTROYED;
     }

     if (!ref->fake.refs) {
          pthread_mutex_unlock (&ref->fake.lock);
          return DFB_BUG;
     }

     if (ref->fake.call) {
          pthread_mutex_unlock (&ref->fake.lock);
          return DFB_BUSY;
     }

     ref->fake.call     = call;
     ref->fake.call_arg = call_arg;

     pthread_mutex_unlock (&ref->fake.lock);

     return DFB_OK;
}

DirectResult
fusion_ref_inherit (FusionRef *ref, FusionRef *from)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( from != NULL );

     /* FIXME */
     return fusion_ref_up( ref, true );
}

DirectResult
fusion_ref_destroy (FusionRef *ref)
{
     D_ASSERT( ref != NULL );

     ref->fake.destroyed = true;

     if (ref->fake.waiting)
          pthread_cond_broadcast (&ref->fake.cond);

     pthread_mutex_unlock (&ref->fake.lock);
     pthread_cond_destroy (&ref->fake.cond);

     return DFB_OK;
}

#endif

