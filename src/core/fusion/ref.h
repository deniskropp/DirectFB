/*
   (c) Copyright 2001  Denis Oliver Kropp <dok@directfb.org>
   All rights reserved.

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

#include <config.h>
#include "fusion_types.h"


  struct _FusionRef {
    int sem_id;
  };


  /*
   * Initialize.
   */
  FusionResult ref_init         (FusionRef *ref);

  /*
   * Lock, increase, unlock.
   */
  FusionResult ref_up           (FusionRef *ref);

  /*
   * Lock, decrease, unlock.
   */
  FusionResult ref_down         (FusionRef *ref);

  /*
   * Wait for zero and lock.
   */
  FusionResult ref_zero_lock    (FusionRef *ref);

  /*
   * Check for zero and lock if true.
   */
  FusionResult ref_zero_trylock (FusionRef *ref);

  /*
   * Unlock the counter.
   * Only to be called after successful zero_lock or zero_trylock.
   */
  FusionResult ref_unlock       (FusionRef *ref);

  /*
   * Deinitialize.
   * Can be called after successful zero_lock or zero_trylock
   * so that waiting ref_up calls return with FUSION_DESTROYED.
   */
  FusionResult ref_destroy      (FusionRef *ref);

#ifdef __cplusplus
}
#endif

#endif /* __REF_H__ */

