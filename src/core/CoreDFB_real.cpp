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

#include <core/CoreDFB.h>

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/core.h>
}

D_DEBUG_DOMAIN( DirectFB_Core, "DirectFB/Core", "DirectFB Core" );

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
ICore_Real::CreateSurface( const CoreSurfaceConfig  *config,
                           CoreSurfaceTypeFlags      type,
                           unsigned long             resource_id,
                           CorePalette              *palette,
                           CoreSurface             **ret_surface )
{
     D_DEBUG_AT( DirectFB_Core, "%s( %p )\n", __FUNCTION__, core );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( config != NULL );
     D_ASSERT( ret_surface != NULL );

     return dfb_surface_create( obj, config, type, resource_id, palette, ret_surface );
}

DFBResult
ICore_Real::CreatePalette( u32           size,
                           CorePalette **ret_palette )
{
     D_DEBUG_AT( DirectFB_Core, "%s( %p )\n", __FUNCTION__, core );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( ret_palette != NULL );

     return dfb_palette_create( obj, size, ret_palette );
}


}

