/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#include <config.h>

#include <core/CoreSlave.h>

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/core.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreSlave, "DirectFB/CoreSlave", "DirectFB Core Slave" );

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
ICoreSlave_Real::GetData(
     void*                                      address,
     u32                                        bytes,
     u8                                        *ret_data
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreSlave, "ICoreSlave_Real::%s( from %lu, address %p, bytes %u )\n",
                __FUNCTION__, Core_GetIdentity(), address, bytes );

    ret = dfb_core_memory_permissions_check( core, CMPF_READ, address, bytes );
    if (ret)
         return ret;

    direct_memcpy( ret_data, address, bytes );

    return DFB_OK;
}

DFBResult
ICoreSlave_Real::PutData(
     void*                                      address,
     u32                                        bytes,
     const u8                                  *data
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreSlave, "ICoreSlave_Real::%s( from %lu, address %p, bytes %u )\n",
                __FUNCTION__, Core_GetIdentity(), address, bytes );

    ret = dfb_core_memory_permissions_check( core, CMPF_WRITE, address, bytes );
    if (ret)
         return ret;

    direct_memcpy( address, data, bytes );

    return DFB_OK;
}


}

