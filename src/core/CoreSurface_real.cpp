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
#include <core/surface.h>
#include <core/surface_pool.h>
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
ISurface_Real::GetPalette(
                         CorePalette                              **ret_palette
                         )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     D_ASSERT( ret_palette != NULL );

     if (!obj->palette)
          return DFB_UNSUPPORTED;

     ret = (DFBResult) dfb_palette_ref( obj->palette );
     if (ret)
          return ret;

     *ret_palette = obj->palette;

     return DFB_OK;
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


static void
manage_interlocks( CoreSurfaceAllocation  *allocation,
                   CoreSurfaceAccessorID   accessor,
                   CoreSurfaceAccessFlags  access )
{
     CoreSurfaceBuffer *buffer;

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

#if 1
     /*
      * Manage access interlocks.
      *
      * SOON FIXME: Clearing flags only when not locked yet. Otherwise nested GPU/CPU locks are a problem.
      */
     /* Software read/write access... */
     if (accessor == CSAID_CPU) {
          /* If hardware has written or is writing... */
          if (allocation->accessed[CSAID_GPU] & CSAF_WRITE) {
               /* ...wait for the operation to finish. */
               dfb_gfxcard_sync(); /* TODO: wait for serial instead */

               /* Software read access after hardware write requires flush of the (bus) read cache. */
               dfb_gfxcard_flush_read_cache();

               if (!buffer->locked) {
                    /* ...clear hardware write access. */
                    allocation->accessed[CSAID_GPU] = (CoreSurfaceAccessFlags)(allocation->accessed[CSAID_GPU] & ~CSAF_WRITE);

                    /* ...clear hardware read access (to avoid syncing twice). */
                    allocation->accessed[CSAID_GPU] = (CoreSurfaceAccessFlags)(allocation->accessed[CSAID_GPU] & ~CSAF_READ);
               }
          }

          /* Software write access... */
          if (access & CSAF_WRITE) {
               /* ...if hardware has (to) read... */
               if (allocation->accessed[CSAID_GPU] & CSAF_READ) {
                    /* ...wait for the operation to finish. */
                    dfb_gfxcard_sync(); /* TODO: wait for serial instead */

                    /* ...clear hardware read access. */
                    if (!buffer->locked)
                         allocation->accessed[CSAID_GPU] = (CoreSurfaceAccessFlags)(allocation->accessed[CSAID_GPU] & ~CSAF_READ);
               }
          }
     }

     /* Hardware read access... */
     if (accessor == CSAID_GPU && access & CSAF_READ) {
          /* ...if software has written before... */
          if (allocation->accessed[CSAID_CPU] & CSAF_WRITE) {
               /* ...flush texture cache. */
               dfb_gfxcard_flush_texture_cache();

               /* ...clear software write access. */
               if (!buffer->locked)
                    allocation->accessed[CSAID_CPU] = (CoreSurfaceAccessFlags)(allocation->accessed[CSAID_CPU] & ~CSAF_WRITE);
          }
     }

     if (! D_FLAGS_ARE_SET( allocation->accessed[accessor], access )) {
          /* FIXME: surface_enter */
     }
#endif

     /* Collect... */
     allocation->accessed[accessor] = (CoreSurfaceAccessFlags)(allocation->accessed[accessor] | access);
}

DFBResult
ISurface_Real::PreLockBuffer(
                         u32                                        buffer_index,
                         CoreSurfaceAccessorID                      accessor,
                         CoreSurfaceAccessFlags                     access,
                         u32                                       *ret_allocation_index
                         )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     CoreSurface           *surface    = obj;
     bool                   allocated  = false;

     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     if (buffer_index >= (u32)surface->num_buffers)
          return DFB_INVARG;

     dfb_surface_lock( surface );

     buffer = surface->buffers[buffer_index];
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Look for allocation with proper access. */
     allocation = dfb_surface_buffer_find_allocation( buffer, accessor, access, true );
     if (!allocation) {
          /* If no allocation exists, create one. */
          ret = dfb_surface_pools_allocate( buffer, accessor, access, &allocation );
          if (ret) {
               if (ret != DFB_NOVIDEOMEMORY && ret != DFB_UNSUPPORTED)
                    D_DERROR( ret, "Core/SurfBuffer: Buffer allocation failed!\n" );

               goto out;
          }

          allocated = true;
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, access );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_pool_deallocate( allocation->pool, allocation );
          goto out;
     }

     manage_interlocks( allocation, accessor, access );

     *ret_allocation_index = fusion_vector_index_of( &buffer->allocs, allocation );

out:
     dfb_surface_unlock( surface );

     return ret;
}

DFBResult
ISurface_Real::PreReadBuffer(
                         u32                                        buffer_index,
                         const DFBRectangle                        *rect,
                         u32                                       *ret_allocation_index
                         )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     CoreSurface           *surface    = obj;
     bool                   allocated  = false;

     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     if (buffer_index >= (u32)surface->num_buffers)
          return DFB_INVARG;

     dfb_surface_lock( surface );

     buffer = surface->buffers[buffer_index];
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Use last written allocation if it's up to date... */
     if (buffer->written && direct_serial_check( &buffer->written->serial, &buffer->serial ))
          allocation = buffer->written;
     else {
          /* ...otherwise look for allocation with CPU access. */
          allocation = dfb_surface_buffer_find_allocation( buffer, CSAID_CPU, CSAF_READ, false );
          if (!allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( buffer, CSAID_CPU, CSAF_READ, &allocation );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Buffer allocation failed!\n" );
                    goto out;
               }

               allocated = true;
          }
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, CSAF_READ );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_pool_deallocate( allocation->pool, allocation );
          goto out;
     }

     if (!(allocation->pool->desc.caps & CSPCAPS_READ))
          manage_interlocks( allocation, CSAID_CPU, CSAF_READ );

     *ret_allocation_index = fusion_vector_index_of( &buffer->allocs, allocation );

out:
     dfb_surface_unlock( surface );

     return ret;
}

DFBResult
ISurface_Real::PreWriteBuffer(
                         u32                                        buffer_index,
                         const DFBRectangle                        *rect,
                         u32                                       *ret_allocation_index
                         )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     CoreSurface           *surface    = obj;
     bool                   allocated  = false;

     D_DEBUG_AT( DirectFB_CoreSurface, "ISurface_Real::%s()\n", __FUNCTION__ );

     if (buffer_index >= (u32)surface->num_buffers)
          return DFB_INVARG;

     dfb_surface_lock( surface );

     buffer = surface->buffers[buffer_index];
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Use last read allocation if it's up to date... */
     if (buffer->read && direct_serial_check( &buffer->read->serial, &buffer->serial ))
          allocation = buffer->read;
     else {
          /* ...otherwise look for allocation with CPU access. */
          allocation = dfb_surface_buffer_find_allocation( buffer, CSAID_CPU, CSAF_WRITE, false );
          if (!allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( buffer, CSAID_CPU, CSAF_WRITE, &allocation );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Buffer allocation failed!\n" );
                    goto out;
               }

               allocated = true;
          }
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, CSAF_WRITE );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_pool_deallocate( allocation->pool, allocation );
          goto out;
     }

     if (!(allocation->pool->desc.caps & CSPCAPS_WRITE))
          manage_interlocks( allocation, CSAID_CPU, CSAF_WRITE );

     *ret_allocation_index = fusion_vector_index_of( &buffer->allocs, allocation );

out:
     dfb_surface_unlock( surface );

     return ret;
}


}

