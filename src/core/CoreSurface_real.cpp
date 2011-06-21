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

#include "CoreSurface.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreSurface, "DirectFB/CoreSurface", "DirectFB CoreSurface" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
ISurface_Real::SetConfig(
                        const CoreSurfaceConfig                   *config
                        )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     D_ASSERT( config != NULL );

     return dfb_surface_reconfig( obj, config );
}


DFBResult
ISurface_Real::LockBuffer(
                         CoreSurfaceBufferRole                      role,
                         CoreSurfaceAccessorID                      accessor,
                         CoreSurfaceAccessFlags                     access,
                         CoreSurfaceBufferLock                     *ret_lock
                         )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     D_ASSERT( ret_lock != NULL );

     return dfb_surface_lock_buffer( obj, role, accessor, access, ret_lock );
}


DFBResult
ISurface_Real::UnlockBuffer(
                           CoreSurfaceBufferLock                     *lock
                           )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     D_ASSERT( lock != NULL );

     return dfb_surface_unlock_buffer( obj, lock );
}


DFBResult
ISurface_Real::Flip(
                   bool                                       swap
                   )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     dfb_surface_lock( obj );

     ret = dfb_surface_flip( obj, swap );

     dfb_surface_unlock( obj );

     return ret;
}


DFBResult
ISurface_Real::SetPalette(
                         CorePalette                               *palette
                         )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     D_ASSERT( palette != NULL );

     return dfb_surface_set_palette( obj, palette );
}


}

