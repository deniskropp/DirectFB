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


/*
 * Initializes fusion and returns the fusion id or -1 on failure.
 */
int fusion_init( int world, int abi_version, int *world_ret );

/*
 * Deinitializes fusion.
 *
 * If 'emergency' is true the function won't join but kill the dispatcher thread.
 */
void fusion_exit( bool emergency );

/*
 * Return the current Fusion ID.
 */
int fusion_id();

/*
 * Processes pending fusion messages.
 */
void fusion_sync();

/*
 * Sends a signal to one or more fusionees and optionally waits
 * for their processes to terminate.
 *
 * A fusion_id of zero means all fusionees but the calling one.
 * A timeout of zero means infinite waiting while a negative value
 * means no waiting at all.
 */
FusionResult fusion_kill( int fusion_id, int signal, int timeout_ms );

/*
 * Get the number of milliseconds passed after the start of the master.
 */
long long fusion_get_millis();


#endif

