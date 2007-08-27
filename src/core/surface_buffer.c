/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <string.h>

#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/memcpy.h>

#include <fusion/shmalloc.h>

#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>


D_DEBUG_DOMAIN( Core_SurfBuffer, "Core/SurfBuffer", "DirectFB Core Surface Buffer" );

/**********************************************************************************************************************/

static DFBResult allocate_buffer( CoreSurfaceBuffer       *buffer,
                                  CoreSurfaceAccessFlags   access,
                                  CoreSurfaceAllocation  **ret_allocation );

static DFBResult update_allocation( CoreSurfaceAllocation  *allocation,
                                    CoreSurfaceAccessFlags  access );

/**********************************************************************************************************************/

DFBResult
dfb_surface_buffer_new( CoreSurface             *surface,
                        CoreSurfaceBufferFlags   flags,
                        CoreSurfaceBuffer      **ret_buffer )
{
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_FLAGS_ASSERT( flags, CSBF_ALL );
     D_ASSERT( ret_buffer != NULL );

#if DIRECT_BUILD_DEBUG
     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_new( %s )\n", dfb_pixelformat_name( surface->config.format ) );

     if (flags & CSBF_STICKED)
          D_DEBUG_AT( Core_SurfBuffer, "  -> STICKED\n" );
#endif

     buffer = SHCALLOC( surface->shmpool, 1, sizeof(CoreSurfaceBuffer) );
     if (!buffer)
          return D_OOSHM();

     direct_serial_init( &buffer->serial );
     direct_serial_increase( &buffer->serial );

     buffer->surface = surface;
     buffer->flags   = flags;
     buffer->format  = surface->config.format;

     if (surface->config.caps & DSCAPS_VIDEOONLY)
          buffer->policy = CSP_VIDEOONLY;
     else if (surface->config.caps & DSCAPS_SYSTEMONLY)
          buffer->policy = CSP_SYSTEMONLY;
     else
          buffer->policy = CSP_VIDEOLOW;

     fusion_vector_init( &buffer->allocs, 2, surface->shmpool );

     D_MAGIC_SET( buffer, CoreSurfaceBuffer );

     *ret_buffer = buffer;

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_destroy( CoreSurfaceBuffer *buffer )
{
     CoreSurface           *surface;
     CoreSurfaceAllocation *allocation;
     int                    i;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_destroy( %p [%dx%d] )\n",
                 buffer, surface->config.size.w, surface->config.size.h );

     fusion_vector_foreach_reverse (allocation, i, buffer->allocs)
          dfb_surface_pool_deallocate( allocation->pool, allocation );

     fusion_vector_destroy( &buffer->allocs );

     direct_serial_deinit( &buffer->serial );

     D_MAGIC_CLEAR( buffer );

     SHFREE( surface->shmpool, buffer );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_lock( CoreSurfaceBuffer      *buffer,
                         CoreSurfaceAccessFlags  access,
                         CoreSurfaceBufferLock  *lock )
{
     DFBResult              ret;
     int                    i;
     CoreSurface           *surface;
     CoreSurfaceAllocation *alloc      = NULL;
     CoreSurfaceAllocation *allocation = NULL;

#if DIRECT_BUILD_DEBUG
     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_lock( %p, 0x%08x, %p )\n", buffer, access, lock );

     if (access & CSAF_GPU_READ)
          D_DEBUG_AT( Core_SurfBuffer, "  -> GPU READ\n" );
     if (access & CSAF_GPU_WRITE)
          D_DEBUG_AT( Core_SurfBuffer, "  -> GPU WRITE\n" );
     if (access & CSAF_CPU_READ)
          D_DEBUG_AT( Core_SurfBuffer, "  -> CPU READ\n" );
     if (access & CSAF_CPU_WRITE)
          D_DEBUG_AT( Core_SurfBuffer, "  -> CPU WRITE\n" );
     if (access & CSAF_SHARED)
          D_DEBUG_AT( Core_SurfBuffer, "  -> PROCESS SHARED\n" );
#endif

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_FLAGS_ASSERT( access, CSAF_ALL );
     D_ASSERT( lock != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     fusion_vector_foreach (alloc, i, buffer->allocs) {
          D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );

          if (D_FLAGS_ARE_SET( alloc->access, access )) {
               /* Take last up to date or first available. */
               if (!allocation || direct_serial_check( &alloc->serial, &buffer->serial ))
                    allocation = alloc;
               //break;
          }
     }

     if (!allocation) {
          ret = allocate_buffer( buffer, access, &allocation );
          if (ret)
               return ret;

          D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     }

     /* Synchronize with other allocations. */
     ret = update_allocation( allocation, access );
     if (ret) {
          /* Destroy if newly created. */
          if (!alloc)
               dfb_surface_pool_deallocate( allocation->pool, allocation );
          return ret;
     }

     lock->buffer     = buffer;
     lock->access     = access;
     lock->allocation = NULL;

     D_MAGIC_SET( lock, CoreSurfaceBufferLock );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                    allocation->pool->desc.name );
          D_MAGIC_CLEAR( lock );

          /* Destroy if newly created. */
          if (!alloc)
               dfb_surface_pool_deallocate( allocation->pool, allocation );

          return ret;
     }

     lock->allocation = allocation;


     /*
      * Manage access interlocks.
      */
     /* Software read/write access... */
     if (access & (CSAF_CPU_READ | CSAF_CPU_WRITE)) {
          /* If hardware has written or is writing... */
          if (allocation->accessed & CSAF_GPU_WRITE) {
               /* ...wait for the operation to finish. */
               dfb_gfxcard_sync(); /* TODO: wait for serial instead */

               /* Software read access after hardware write requires flush of the (bus) read cache. */
               if (access & CSAF_CPU_READ)
                    dfb_gfxcard_flush_read_cache();

               /* ...clear hardware write access. */
               allocation->accessed &= ~CSAF_GPU_WRITE;
          }

          /* Software write access... */
          if (access & CSAF_CPU_WRITE) {
               /* ...if hardware has (to) read... */
               if (allocation->accessed & CSAF_GPU_READ) {
                    /* ...wait for the operation to finish. */
                    dfb_gfxcard_sync(); /* TODO: wait for serial instead */

                    /* ...clear hardware read access. */
                    allocation->accessed &= ~CSAF_GPU_READ;
               }
          }
     }

     /* Hardware read access... */
     if (access & CSAF_GPU_READ) {
          /* ...if software has written before... */
          if (allocation->accessed & CSAF_CPU_WRITE) {
               /* ...flush texture cache. */
               dfb_gfxcard_flush_texture_cache();

               /* ...clear software write access. */
               allocation->accessed &= ~CSAF_CPU_WRITE;
          }
     }

     if (! D_FLAGS_ARE_SET( allocation->accessed, access )) {
          /* surface_enter */
     }

     /* Collect... */
     allocation->accessed |= access;

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_unlock( CoreSurfaceBufferLock *lock )
{
     DFBResult        ret;
     CoreSurfacePool *pool;

     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_unlock( %p )\n", lock );

     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_MAGIC_ASSERT( lock->buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( lock->buffer->surface, CoreSurface );
     D_MAGIC_ASSERT( lock->allocation, CoreSurfaceAllocation );

     FUSION_SKIRMISH_ASSERT( &lock->buffer->surface->lock );

     pool = lock->allocation->pool;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     ret = dfb_surface_pool_unlock( pool, lock->allocation, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Unlocking allocation failed! [%s]\n", pool->desc.name );
          return ret;
     }

     lock->buffer     = NULL;
     lock->allocation = NULL;

     lock->addr       = NULL;
     lock->phys       = 0;
     lock->offset     = 0;
     lock->pitch      = 0;
     lock->handle     = NULL;

     D_MAGIC_CLEAR( lock );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_read( CoreSurfaceBuffer  *buffer,
                         void               *destination,
                         int                 pitch,
                         const DFBRectangle *prect )
{
     DFBResult              ret;
     DFBRectangle           rect;
     int                    i, y;
     int                    bytes;
     CoreSurface           *surface;
     CoreSurfaceBufferLock  lock;
     CoreSurfaceAllocation *alloc;
     CoreSurfaceAllocation *allocation = NULL;
     DFBSurfacePixelFormat  format;

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p [%d] )\n", __FUNCTION__, buffer, destination, pitch );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( destination != NULL );
     D_ASSERT( pitch > 0 );
     DFB_RECTANGLE_ASSERT_IF( prect );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     /* Determine area. */
     rect.x = 0;
     rect.y = 0;
     rect.w = surface->config.size.w;
     rect.h = surface->config.size.h;

     if (prect && (!dfb_rectangle_intersect( &rect, prect ) || !DFB_RECTANGLE_EQUAL( rect, *prect )))
          return DFB_INVAREA;

     /* Calculate bytes per read line. */
     format = surface->config.format;
     bytes  = DFB_BYTES_PER_LINE( format, rect.w );

     D_DEBUG_AT( Core_SurfBuffer, "  -> %d,%d - %dx%d (%s)\n", DFB_RECTANGLE_VALS(&rect),
                 dfb_pixelformat_name( format ) );

     /* If no allocations exists, simply clear the destination. */
     if (!buffer->allocs.elements) {
          for (y=0; y<rect.h; y++) {
               memset( destination, 0, bytes );

               destination += pitch;
          }

          return DFB_OK;
     }

     /* Look for allocation with CPU access. */
     fusion_vector_foreach (alloc, i, buffer->allocs) {
          if (D_FLAGS_ARE_SET( alloc->access, CSAF_CPU_READ )) {
               allocation = alloc;
               break;
          }
     }

     /* FIXME: use Read() */
     if (!allocation)
          return DFB_UNIMPLEMENTED;

     /* Synchronize with other allocations. */
     ret = update_allocation( allocation, CSAF_CPU_READ );
     if (ret)
          return ret;

     /* Lock the allocation. */
     lock.buffer = buffer;
     lock.access = CSAF_CPU_READ;

     D_MAGIC_SET( &lock, CoreSurfaceBufferLock );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                    allocation->pool->desc.name );
          D_MAGIC_CLEAR( &lock );
          return ret;
     }

     /* Move to start of read. */
     lock.addr += DFB_BYTES_PER_LINE( format, rect.x ) + rect.y * lock.pitch;

     /* Copy the data. */
     for (y=0; y<rect.h; y++) {
          direct_memcpy( destination, lock.addr, bytes );

          destination += pitch;
          lock.addr   += lock.pitch;
     }

     /* Unlock the allocation. */
     ret = dfb_surface_pool_unlock( allocation->pool, allocation, &lock );
     if (ret)
          D_DERROR( ret, "Core/SurfBuffer: Unlocking allocation failed! [%s]\n", allocation->pool->desc.name );

     D_MAGIC_CLEAR( &lock );

     return ret;
}

DFBResult
dfb_surface_buffer_write( CoreSurfaceBuffer  *buffer,
                          const void         *source,
                          int                 pitch,
                          const DFBRectangle *prect )
{
     DFBResult              ret;
     DFBRectangle           rect;
     int                    i, y;
     int                    bytes;
     CoreSurface           *surface;
     CoreSurfaceBufferLock  lock;
     CoreSurfaceAllocation *alloc      = NULL;
     CoreSurfaceAllocation *allocation = NULL;
     DFBSurfacePixelFormat  format;

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p [%d] )\n", __FUNCTION__, buffer, source, pitch );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( pitch > 0 || source == NULL );
     DFB_RECTANGLE_ASSERT_IF( prect );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     /* Determine area. */
     rect.x = 0;
     rect.y = 0;
     rect.w = surface->config.size.w;
     rect.h = surface->config.size.h;

     if (prect && (!dfb_rectangle_intersect( &rect, prect ) || !DFB_RECTANGLE_EQUAL( rect, *prect )))
          return DFB_INVAREA;

     /* Calculate bytes per written line. */
     format = surface->config.format;
     bytes  = DFB_BYTES_PER_LINE( format, rect.w );

     D_DEBUG_AT( Core_SurfBuffer, "  -> %d,%d - %dx%d (%s)\n", DFB_RECTANGLE_VALS(&rect),
                 dfb_pixelformat_name( format ) );

     /* If no allocations exists, create one. */
     if (!buffer->allocs.elements) {
          ret = allocate_buffer( buffer, CSAF_CPU_WRITE, &allocation );
          if (ret) {
               D_DERROR( ret, "Core/SurfBuffer: Buffer allocation failed!\n" );
               return ret;
          }
     }
     else {
          /* Look for allocation with CPU access. */
          fusion_vector_foreach (alloc, i, buffer->allocs) {
               if (D_FLAGS_ARE_SET( alloc->access, CSAF_CPU_WRITE )) {
                    allocation = alloc;
                    break;
               }
          }
     }

     /* FIXME: use Write() */
     if (!allocation)
          return DFB_UNIMPLEMENTED;

     /* Synchronize with other allocations. */
     ret = update_allocation( allocation, CSAF_CPU_WRITE );
     if (ret) {
          /* Destroy if newly created. */
          if (!alloc)
               dfb_surface_pool_deallocate( allocation->pool, allocation );
          return ret;
     }

     /* Lock the allocation. */
     lock.buffer = buffer;
     lock.access = CSAF_CPU_WRITE;

     D_MAGIC_SET( &lock, CoreSurfaceBufferLock );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                    allocation->pool->desc.name );
          D_MAGIC_CLEAR( &lock );
          return ret;
     }

     /* Move to start of write. */
     lock.addr += DFB_BYTES_PER_LINE( format, rect.x ) + rect.y * lock.pitch;

     /* Copy the data. */
     for (y=0; y<rect.h; y++) {
          if (source) {
               direct_memcpy( lock.addr, source, bytes );

               source += pitch;
          }
          else
               memset( lock.addr, 0, bytes );

          lock.addr += lock.pitch;
     }

     /* Unlock the allocation. */
     ret = dfb_surface_pool_unlock( allocation->pool, allocation, &lock );
     if (ret)
          D_DERROR( ret, "Core/SurfBuffer: Unlocking allocation failed! [%s]\n", allocation->pool->desc.name );

     D_MAGIC_CLEAR( &lock );

     return ret;
}

DFBResult
dfb_surface_buffer_dump( CoreSurfaceBuffer *buffer,
                         const char        *directory,
                         const char        *prefix )
{
     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_dump( %p, %p, %p )\n", buffer, directory, prefix );

     D_UNIMPLEMENTED();

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
allocate_buffer( CoreSurfaceBuffer       *buffer,
                 CoreSurfaceAccessFlags   access,
                 CoreSurfaceAllocation  **ret_allocation )
{
     DFBResult              ret;
     CoreSurface           *surface;
     CoreSurfacePool       *pool;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_FLAGS_ASSERT( access, CSAF_ALL );
     D_ASSERT( ret_allocation != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, 0x%x )\n", __FUNCTION__, buffer, access );

     D_DEBUG_AT( Core_SurfBuffer, " -> %dx%d %s - %s%s%s%s%s%s%s\n",
                 surface->config.size.w, surface->config.size.h,
                 dfb_pixelformat_name( surface->config.format ),
                 (surface->type & CSTF_SHARED)   ? "SHARED "  : "PRIVATE ",
                 (surface->type & CSTF_LAYER)    ? "LAYER "   : "",
                 (surface->type & CSTF_WINDOW)   ? "WINDOW "  : "",
                 (surface->type & CSTF_CURSOR)   ? "CURSOR "  : "",
                 (surface->type & CSTF_FONT)     ? "FONT "    : "",
                 (surface->type & CSTF_INTERNAL) ? "INTERNAL" : "",
                 (surface->type & CSTF_EXTERNAL) ? "EXTERNAL" : "" );

     ret = dfb_surface_pools_negotiate( buffer, access, &pool );
     if (ret) {
          D_DEBUG_AT( Core_SurfBuffer, " -> failed! (%s)\n", DirectFBErrorString( ret ) );
          return ret;
     }

     ret = dfb_surface_pool_allocate( pool, buffer, &allocation );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Allocation in '%s' failed!\n", pool->desc.name );
          return ret;
     }

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     *ret_allocation = allocation;

     return DFB_OK;
}

static void
transfer_buffer( CoreSurfaceBuffer *buffer,
                 void              *src,
                 void              *dst,
                 int                srcpitch,
                 int                dstpitch )
{
     int          i;
     CoreSurface *surface;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p [%d] -> %p [%d] ) * %d\n",
                 __FUNCTION__, buffer, src, srcpitch, dst, dstpitch, surface->config.size.h );

     D_ASSERT( src != NULL );
     D_ASSERT( dst != NULL );
     D_ASSERT( srcpitch > 0 );
     D_ASSERT( dstpitch > 0 );

     D_ASSERT( srcpitch >= DFB_BYTES_PER_LINE( buffer->format, surface->config.size.w ) );
     D_ASSERT( dstpitch >= DFB_BYTES_PER_LINE( buffer->format, surface->config.size.w ) );

     for (i=0; i<surface->config.size.h; i++) {
          direct_memcpy( dst, src, DFB_BYTES_PER_LINE( buffer->format, surface->config.size.w ) );

          src += srcpitch;
          dst += dstpitch;
     }

     switch (buffer->format) {
          case DSPF_YV12:
          case DSPF_I420:
               for (i=0; i<surface->config.size.h; i++) {
                    direct_memcpy( dst, src,
                                   DFB_BYTES_PER_LINE( buffer->format, surface->config.size.w / 2 ) );
                    src += srcpitch / 2;
                    dst += dstpitch / 2;
               }
               break;

          case DSPF_NV12:
          case DSPF_NV21:
               for (i=0; i<surface->config.size.h/2; i++) {
                    direct_memcpy( dst, src,
                                   DFB_BYTES_PER_LINE( buffer->format, surface->config.size.w ) );
                    src += srcpitch;
                    dst += dstpitch;
               }
               break;

          case DSPF_NV16:
               for (i=0; i<surface->config.size.h; i++) {
                    direct_memcpy( dst, src,
                                   DFB_BYTES_PER_LINE( buffer->format, surface->config.size.w ) );
                    src += srcpitch;
                    dst += dstpitch;
               }
               break;

          default:
               break;
     }
}

static DFBResult
update_allocation( CoreSurfaceAllocation  *allocation,
                   CoreSurfaceAccessFlags  access )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_DEBUG_AT( Core_SurfBuffer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_FLAGS_ASSERT( access, CSAF_ALL );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     if (direct_serial_update( &allocation->serial, &buffer->serial ) && buffer->written) {
          CoreSurfaceAllocation *source = buffer->written;

          D_DEBUG_AT( Core_SurfBuffer, "  -> updating allocation...\n" );

          D_MAGIC_ASSERT( source, CoreSurfaceAllocation );
          D_ASSERT( source->buffer == allocation->buffer );

          if ((source->access & CSAF_CPU_READ) && (allocation->access & CSAF_CPU_WRITE)) {
               CoreSurfaceBufferLock src;
               CoreSurfaceBufferLock dst;

               D_MAGIC_SET( &src, CoreSurfaceBufferLock );

               src.buffer = buffer;
               src.access = CSAF_CPU_READ;

               ret = dfb_surface_pool_lock( source->pool, source, &src );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Could not lock source for transfer!\n" );
                    D_MAGIC_CLEAR( &src );
                    return ret;
               }

               D_MAGIC_SET( &dst, CoreSurfaceBufferLock );

               dst.buffer = buffer;
               dst.access = CSAF_CPU_WRITE;

               ret = dfb_surface_pool_lock( allocation->pool, allocation, &dst );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Could not lock destination for transfer!\n" );
                    dfb_surface_pool_unlock( source->pool, source, &src );
                    return ret;
               }

               transfer_buffer( buffer, src.addr, dst.addr, src.pitch, dst.pitch );

               dfb_surface_pool_unlock( allocation->pool, allocation, &dst );
               dfb_surface_pool_unlock( source->pool, source, &src );

               D_MAGIC_CLEAR( &dst );
               D_MAGIC_CLEAR( &src );
          }
          else {
               D_UNIMPLEMENTED();
          }
     }

     if (access & (CSAF_CPU_WRITE | CSAF_GPU_WRITE)) {
          D_DEBUG_AT( Core_SurfBuffer, "  -> increasing serial...\n" );

          direct_serial_increase( &buffer->serial );

          direct_serial_copy( &allocation->serial, &buffer->serial );

          buffer->written = allocation;
     }

     return DFB_OK;
}

