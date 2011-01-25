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

#ifndef __FUSION__PROPERTY_H__
#define __FUSION__PROPERTY_H__

#include <direct/thread.h>

#include <fusion/types.h>

typedef enum {
     FUSION_PROPERTY_AVAILABLE,
     FUSION_PROPERTY_LEASED,
     FUSION_PROPERTY_PURCHASED
} FusionPropertyState;


typedef union {
     /* multi app */
     struct {
          int                      id;
          const FusionWorldShared *shared;
          /* builtin impl */
          struct {
               FusionPropertyState state;
               pid_t               owner;
               bool                requested;
               bool                destroyed;
          } builtin;
     } multi;
     
     /* single app */
     struct {
          DirectMutex              lock;
          DirectWaitQueue          cond;
          FusionPropertyState      state;
     } single;
} FusionProperty;

/*
 * Initializes the property
 */
DirectResult FUSION_API fusion_property_init     (FusionProperty    *property,
                                                  const FusionWorld *world);

/*
 * Lease the property causing others to wait before leasing or purchasing.
 *
 * Waits as long as property is leased by another party.
 * Returns DR_BUSY if property is/gets purchased by another party.
 *
 * Succeeds if property is available,
 * puts the property into 'leased' state.
 */
DirectResult FUSION_API fusion_property_lease    (FusionProperty *property);

/*
 * Purchase the property disallowing others to lease or purchase it.
 *
 * Waits as long as property is leased by another party.
 * Returns DR_BUSY if property is/gets purchased by another party.
 *
 * Succeeds if property is available,
 * puts the property into 'purchased' state and wakes up any waiting party.
 */
DirectResult FUSION_API fusion_property_purchase (FusionProperty *property);

/*
 * Cede the property allowing others to lease or purchase it.
 *
 * Puts the property into 'available' state and wakes up one waiting party.
 */
DirectResult FUSION_API fusion_property_cede     (FusionProperty *property);

/*
 * Kills the owner of the property.
 *
 * Tries to make a purchased property available again by killing
 * the process that purchased it.
 */
DirectResult FUSION_API fusion_property_holdup   (FusionProperty *property);

/*
 * Destroys the property
 */
DirectResult FUSION_API fusion_property_destroy  (FusionProperty *property);

#endif

