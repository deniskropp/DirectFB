/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#ifndef __FUSION__REF_H__
#define __FUSION__REF_H__

#include <direct/thread.h>

#include <fusion/types.h>
#include <fusion/call.h>
#include <fusion/lock.h>

typedef union {
     /* multi app */
     struct {
          int                      id;
          const FusionWorldShared *shared;
          FusionID                 creator;
          /* builtin impl */
          struct {
               int                 local;
               int                 global;
               FusionSkirmish      lock; 
          
               FusionCall         *call;
               int                 call_arg;
          } builtin;
     } multi;
     
     /* single app */
     struct {
          int                      refs;
          DirectWaitQueue          cond;
          DirectMutex              lock;
          bool                     destroyed;
          int                      locked;

          FusionCall              *call;
          int                      call_arg;
     } single;
} FusionRef;

/*
 * Initialize.
 */
DirectResult FUSION_API fusion_ref_init         (FusionRef         *ref,
                                                 const char        *name,
                                                 const FusionWorld *world);

DirectResult FUSION_API fusion_ref_set_name     (FusionRef         *ref,
                                                 const char        *name);

/*
 * Lock, increase, unlock.
 */
DirectResult FUSION_API fusion_ref_up           (FusionRef *ref, bool global);

/*
 * Lock, decrease, unlock.
 */
DirectResult FUSION_API fusion_ref_down         (FusionRef *ref, bool global);

/*
 * Catch reference
 */
DirectResult FUSION_API fusion_ref_catch        (FusionRef *ref);

/*
 * Throw reference
 */
DirectResult FUSION_API fusion_ref_throw        (FusionRef *ref, FusionID catcher);

/*
 * Get the current reference count. Meant for debugging only.
 * This value is not reliable, because no locking will be performed
 * and the value may change after or even while returning it.
 */
DirectResult FUSION_API fusion_ref_stat         (FusionRef *ref, int *refs);

/*
 * Wait for zero and lock.
 */
DirectResult FUSION_API fusion_ref_zero_lock    (FusionRef *ref);

/*
 * Check for zero and lock if true.
 */
DirectResult FUSION_API fusion_ref_zero_trylock (FusionRef *ref);

/*
 * Unlock the counter.
 * Only to be called after successful zero_lock or zero_trylock.
 */
DirectResult FUSION_API fusion_ref_unlock       (FusionRef *ref);

/*
 * Have the call executed when reference counter reaches zero.
 */
DirectResult FUSION_API fusion_ref_watch        (FusionRef  *ref,
                                                 FusionCall *call,
                                                 int         call_arg);

/*
 * Inherit local reference count from another reference.
 *
 * The local count of the other reference (and its inherited references) is added to this reference.
 */
DirectResult FUSION_API fusion_ref_inherit      (FusionRef *ref,
                                                 FusionRef *from);

/*
 * Deinitialize.
 * Can be called after successful zero_lock or zero_trylock
 * so that waiting fusion_ref_up calls return with DR_DESTROYED.
 */
DirectResult FUSION_API fusion_ref_destroy      (FusionRef *ref);


typedef enum {
     FUSION_REF_PERMIT_NONE                  = 0x00000000,

     FUSION_REF_PERMIT_REF_UNREF_LOCAL       = 0x00000001,
     FUSION_REF_PERMIT_REF_UNREF_GLOBAL      = 0x00000002,
     FUSION_REF_PERMIT_ZERO_LOCK_UNLOCK      = 0x00000004,
     FUSION_REF_PERMIT_WATCH                 = 0x00000008,
     FUSION_REF_PERMIT_INHERIT               = 0x00000010,
     FUSION_REF_PERMIT_DESTROY               = 0x00000020,
     FUSION_REF_PERMIT_CATCH                 = 0x00000040,
     FUSION_REF_PERMIT_THROW                 = 0x00000080,

     FUSION_REF_PERMIT_ALL                   = 0x000000FF
} FusionRefPermissions;

/*
 * Give permissions to another fusionee to use the reference.
 */
DirectResult  FUSION_API fusion_ref_add_permissions( FusionRef            *ref,
                                                     FusionID              fusion_id,
                                                     FusionRefPermissions  permissions );

#endif

