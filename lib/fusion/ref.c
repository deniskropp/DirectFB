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

D_DEBUG_DOMAIN( Fusion_Ref, "Fusion/Ref", "Fusion's Reference Counter" );


DirectResult
fusion_ref_init (FusionRef         *ref,
                 const char        *name,
                 const FusionWorld *world)
{
     FusionEntryInfo info;

     D_ASSERT( ref != NULL );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Ref, "fusion_ref_init( %p, '%s' )\n", ref, name ? : "" );

     while (ioctl( world->fusion_fd, FUSION_REF_NEW, &ref->multi.id )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "FUSION_REF_NEW" );
          return DFB_FUSION;
     }

     D_DEBUG_AT( Fusion_Ref, "  -> new ref %p [%d]\n", ref, ref->multi.id );

     info.type = FT_REF;
     info.id   = ref->multi.id;

     strncpy( info.name, name, sizeof(info.name) );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );

     /* Keep back pointer to shared world data. */
     ref->multi.shared = world->shared;

     return DFB_OK;
}

DirectResult
fusion_ref_up (FusionRef *ref, bool global)
{
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd( ref->multi.shared ), global ?
                   FUSION_REF_UP_GLOBAL : FUSION_REF_UP, &ref->multi.id))
     {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return DFB_LOCKED;
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
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd( ref->multi.shared ), global ?
                   FUSION_REF_DOWN_GLOBAL : FUSION_REF_DOWN, &ref->multi.id))
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

     D_ASSERT( ref != NULL );
     D_ASSERT( refs != NULL );

     while ((val = ioctl (_fusion_fd( ref->multi.shared ), FUSION_REF_STAT, &ref->multi.id)) < 0) {
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
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd( ref->multi.shared ), FUSION_REF_ZERO_LOCK, &ref->multi.id)) {
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
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd( ref->multi.shared ), FUSION_REF_ZERO_TRYLOCK, &ref->multi.id)) {
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
     D_ASSERT( ref != NULL );

     while (ioctl (_fusion_fd( ref->multi.shared ), FUSION_REF_UNLOCK, &ref->multi.id)) {
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

     D_ASSERT( ref != NULL );
     D_ASSERT( call != NULL );

     watch.id       = ref->multi.id;
     watch.call_id  = call->call_id;
     watch.call_arg = call_arg;

     while (ioctl (_fusion_fd( ref->multi.shared ), FUSION_REF_WATCH, &watch)) {
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

     D_ASSERT( ref != NULL );
     D_ASSERT( from != NULL );

     inherit.id   = ref->multi.id;
     inherit.from = from->multi.id;

     while (ioctl (_fusion_fd( ref->multi.shared ), FUSION_REF_INHERIT, &inherit)) {
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
     D_ASSERT( ref != NULL );

     D_DEBUG_AT( Fusion_Ref, "fusion_ref_destroy( %p [%d] )\n", ref, ref->multi.id );

     while (ioctl (_fusion_fd( ref->multi.shared ), FUSION_REF_DESTROY, &ref->multi.id)) {
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
fusion_ref_init (FusionRef         *ref,
                 const char        *name,
                 const FusionWorld *world)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( name != NULL );

     direct_util_recursive_pthread_mutex_init (&ref->single.lock);
     pthread_cond_init (&ref->single.cond, NULL);

     ref->single.refs      = 0;
     ref->single.destroyed = false;
     ref->single.waiting   = 0;

     return DFB_OK;
}

DirectResult
fusion_ref_up (FusionRef *ref, bool global)
{
     DirectResult ret = DFB_OK;

     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->single.lock);

     if (ref->single.destroyed)
          ret = DFB_DESTROYED;
     else
          ref->single.refs++;

     pthread_mutex_unlock (&ref->single.lock);

     return ret;
}

DirectResult
fusion_ref_down (FusionRef *ref, bool global)
{
     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->single.lock);

     if (!ref->single.refs) {
          D_BUG( "no more references" );
          pthread_mutex_unlock (&ref->single.lock);
          return DFB_BUG;
     }

     if (ref->single.destroyed) {
          pthread_mutex_unlock (&ref->single.lock);
          return DFB_DESTROYED;
     }

     if (! --ref->single.refs && ref->single.call) {
          FusionCall *call = ref->single.call;

          if (call->handler) {
               pthread_mutex_unlock (&ref->single.lock);
               call->handler( 0, ref->single.call_arg, NULL, call->ctx );
               return DFB_OK;
          }
     }

     pthread_mutex_unlock (&ref->single.lock);

     return DFB_OK;
}

DirectResult
fusion_ref_stat (FusionRef *ref, int *refs)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( refs != NULL );

     if (ref->single.destroyed)
          return DFB_DESTROYED;

     *refs = ref->single.refs;

     return DFB_OK;
}

DirectResult
fusion_ref_zero_lock (FusionRef *ref)
{
     DirectResult ret = DFB_OK;

     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->single.lock);

     if (ref->single.destroyed)
          ret = DFB_DESTROYED;
     else if (ref->single.call)
          ret = DFB_ACCESSDENIED;
     else while (ref->single.refs && !ret) {
          ref->single.waiting++;
          pthread_cond_wait (&ref->single.cond, &ref->single.lock);
          ref->single.waiting--;

          if (ref->single.destroyed)
               ret = DFB_DESTROYED;
          else if (ref->single.call)
               ret = DFB_ACCESSDENIED;
     }

     if (ret != DFB_OK)
          pthread_mutex_unlock (&ref->single.lock);

     return ret;
}

DirectResult
fusion_ref_zero_trylock (FusionRef *ref)
{
     DirectResult ret = DFB_OK;

     D_ASSERT( ref != NULL );

     pthread_mutex_lock (&ref->single.lock);

     if (ref->single.destroyed)
          ret = DFB_DESTROYED;
     else if (ref->single.refs)
          ret = DFB_BUSY;

     if (ret != DFB_OK)
          pthread_mutex_unlock (&ref->single.lock);

     return ret;
}

DirectResult
fusion_ref_unlock (FusionRef *ref)
{
     D_ASSERT( ref != NULL );

     pthread_mutex_unlock (&ref->single.lock);

     return DFB_OK;
}

DirectResult
fusion_ref_watch (FusionRef *ref, FusionCall *call, int call_arg)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( call != NULL );

     pthread_mutex_lock (&ref->single.lock);

     if (ref->single.destroyed) {
          pthread_mutex_unlock (&ref->single.lock);
          return DFB_DESTROYED;
     }

     if (!ref->single.refs) {
          pthread_mutex_unlock (&ref->single.lock);
          return DFB_BUG;
     }

     if (ref->single.call) {
          pthread_mutex_unlock (&ref->single.lock);
          return DFB_BUSY;
     }

     ref->single.call     = call;
     ref->single.call_arg = call_arg;

     pthread_mutex_unlock (&ref->single.lock);

     return DFB_OK;
}

DirectResult
fusion_ref_inherit (FusionRef *ref, FusionRef *from)
{
     D_ASSERT( ref != NULL );
     D_ASSERT( from != NULL );

     D_UNIMPLEMENTED();

     /* FIXME */
     return fusion_ref_up( ref, true );
}

DirectResult
fusion_ref_destroy (FusionRef *ref)
{
     D_ASSERT( ref != NULL );

     ref->single.destroyed = true;

     if (ref->single.waiting)
          pthread_cond_broadcast (&ref->single.cond);

     pthread_mutex_unlock (&ref->single.lock);
     pthread_cond_destroy (&ref->single.cond);

     return DFB_OK;
}

#endif

