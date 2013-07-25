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

#include <directfb.h>


extern "C" {
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <core/CoreSurfaceAllocation.h>


D_DEBUG_DOMAIN( DirectFB_CoreSurfaceAllocation, "DirectFB/CoreSurfaceAllocation", "DirectFB CoreSurfaceAllocation" );

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
ISurfaceAllocation_Real::Update(
                   const DFBRegion                              *region
                   )
{
     D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "ISurfaceAllocation_Real::%s( region %p )\n", __FUNCTION__, region );

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

DFBResult
ISurfaceAllocation_Real::Updated( const DFBBox *updates,
                                  u32           num_updates )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "ISurfaceAllocation_Real::%s( obj %p, updates %p, num %u )\n", __FUNCTION__, obj, updates, num_updates );

     ret = (DFBResult) fusion_object_get( core->shared->surface_buffer_pool, obj->buffer_id, (FusionObject**) &buffer );
     if (ret && ret != DFB_DEAD)
          return ret;

     if (ret == DFB_DEAD) {
          D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  -> dead object!\n" );
     }
     else {
          if (obj->buffer) {
               D_ASSERT( obj->buffer == buffer );
          
               D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- buffer  %s\n", *ToString<CoreSurfaceBuffer>( *buffer ) );
               D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- written %p\n", buffer->written );
               D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- read    %p\n", buffer->read );
               D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- serial  %lu (this %lu)\n", buffer->serial.value, obj->serial.value );
               
               direct_serial_increase( &buffer->serial );
               
               direct_serial_copy( &obj->serial, &buffer->serial );
               
               buffer->written = obj;
               buffer->read    = NULL;
               
               D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  -> serial  %lu\n", buffer->serial.value );
          }
          else {
               D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  -> already decoupled!\n" );
          }

          dfb_surface_buffer_unref( buffer );
     }

     return DFB_OK;
}


}

