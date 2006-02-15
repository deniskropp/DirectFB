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

#ifndef __FUSION__FUSION_H__
#define __FUSION__FUSION_H__

#include <fusion/types.h>

typedef enum {
     FER_ANY,
     FER_MASTER,
     FER_SLAVE
} FusionEnterRole;

/*
 * Enters a fusion world by joining or creating it.
 *
 * If <b>world_index</b> is negative, the next free index is used to create a new world.
 * Otherwise the world with the specified index is joined or created.
 */
DirectResult fusion_enter( int               world_index,
                           int               abi_version,
                           FusionEnterRole   role,
                           FusionWorld     **ret_world );

/*
 * Exits the fusion world.
 *
 * If 'emergency' is true the function won't join but kill the dispatcher thread.
 */
DirectResult fusion_exit( FusionWorld *world,
                          bool         emergency );

/*
 * Return the index of the specified world.
 */
int fusion_world_index( const FusionWorld *world );

/*
 * Return the own Fusion ID within the specified world.
 */
FusionID fusion_id( const FusionWorld *world );

/*
 * Return true if this process is the master.
 */
bool fusion_master( const FusionWorld *world );

/*
 * Wait until all pending messages are processed.
 */
DirectResult fusion_sync( const FusionWorld *world );

/*
 * Sends a signal to one or more fusionees and optionally waits
 * for their processes to terminate.
 *
 * A fusion_id of zero means all fusionees but the calling one.
 * A timeout of zero means infinite waiting while a negative value
 * means no waiting at all.
 */
DirectResult fusion_kill( FusionWorld *world,
                          FusionID     fusion_id,
                          int          signal,
                          int          timeout_ms );

/* Check if a pointer points to the shared memory. */
bool fusion_is_shared( FusionWorld *world,
                       const void  *ptr );

#endif

