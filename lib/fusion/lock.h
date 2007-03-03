/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __FUSION__LOCK_H__
#define __FUSION__LOCK_H__

#include <pthread.h>

#include <fusion/types.h>

#include <direct/messages.h>


typedef union {
     /* multi app */
     struct {
          int                      id;
          const FusionWorldShared *shared;
     } multi;

     /* single app */
     struct {
          pthread_mutex_t          lock;
     } single;
} FusionSkirmish;

/*
 * Initialize.
 */
DirectResult fusion_skirmish_init   ( FusionSkirmish    *skirmish,
                                      const char        *name,
                                      const FusionWorld *world );

/*
 * Lock.
 */
DirectResult fusion_skirmish_prevail( FusionSkirmish    *skirmish );

/*
 * Try lock.
 */
DirectResult fusion_skirmish_swoop  ( FusionSkirmish    *skirmish );

/*
 * Find out how many times current thread has acquired lock. 
 */
DirectResult fusion_skirmish_lock_count( FusionSkirmish *skirmish, int *lock_count );

/*
 * Unlock.
 */
DirectResult fusion_skirmish_dismiss( FusionSkirmish    *skirmish );

/*
 * Deinitialize.
 */
DirectResult fusion_skirmish_destroy( FusionSkirmish    *skirmish );


#endif

