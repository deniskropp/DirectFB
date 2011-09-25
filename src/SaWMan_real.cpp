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

#include "SaWMan.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/core.h>
}

D_DEBUG_DOMAIN( DirectFB_SaWMan, "DirectFB/SaWMan", "DirectFB SaWMan" );

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
ISaWManWM_Real::Start(
                    const u8                                  *name,
                    u32                                        name_len,
                    s32                                       *ret_pid
)
{
     int ret;

     D_DEBUG_AT( DirectFB_SaWMan, "%s()", __FUNCTION__ );

     ret = sawman_lock( obj );
     if (ret)
          return (DFBResult) ret;

     ret = sawman_call( obj, SWMCID_START, (void*) name, name_len, false );
     if (ret < 0) {
          if (ret_pid)
               *ret_pid = -ret;
          ret = DFB_OK;
     }

     sawman_unlock( obj );

     return (DFBResult) ret;
}

DFBResult
ISaWManWM_Real::Stop(
                    s32                                        pid
)
{
    DirectResult ret;

    D_DEBUG_AT( DirectFB_SaWMan, "%s()", __FUNCTION__ );

    ret = sawman_lock( obj );
    if (ret)
         return (DFBResult) ret;

    ret = sawman_call( obj, SWMCID_STOP, &pid, sizeof(u32), false );

    sawman_unlock( obj );

    return (DFBResult) ret;
}

DFBResult
ISaWManWM_Real::RegisterProcess(
                    SaWManProcessFlags                         flags,
                    s32                                        pid,
                    u32                                        fusion_id,
                    SaWManProcess                            **ret_process
)
{
    D_DEBUG_AT( DirectFB_SaWMan, "%s()", __FUNCTION__ );

    return (DFBResult) sawman_register_process( obj, flags, pid, fusion_id, dfb_core_world(core), ret_process );
}


}

