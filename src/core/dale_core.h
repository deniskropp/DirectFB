/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __FUSIONDALE_CORE_H__
#define __FUSIONDALE_CORE_H__

#include <fusiondale.h>

#include <fusion/object.h>

#include <core/dale_types.h>

/*
 * Core initialization and deinitialization
 */
DFBResult fd_core_create ( CoreDale **ret_core );
DFBResult fd_core_destroy( CoreDale  *core, bool emergency );

/*
 * Object creation
 */
CoreMessenger     *fd_core_create_messenger     ( CoreDale *core );
CoreMessengerPort *fd_core_create_messenger_port( CoreDale *core );

/*
 * Object enumeration
 */
DirectResult fd_core_enum_messengers     ( CoreDale             *core,
                                           FusionObjectCallback  callback,
                                           void                 *ctx );

DirectResult fd_core_enum_messenger_ports( CoreDale             *core,
                                           FusionObjectCallback  callback,
                                           void                 *ctx );

DirectResult fd_core_get_messenger       ( CoreDale             *core,
                                           FusionObjectID        object_id,
                                           CoreMessenger       **ret_messenger );


/*
 * Returns the Fusion World of the sound core.
 */
FusionWorld *fd_core_world( CoreDale *core );

/*
 * Returns the Fusion Shared Memory Pool of the sound core.
 */
FusionSHMPoolShared *fd_core_shmpool( CoreDale *core );


#endif
