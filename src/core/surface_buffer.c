/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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



//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/memcpy.h>

#include <fusion/shmalloc.h>

#include <core/CoreSurface.h>

#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <core/surface_pool_bridge.h>

#include <misc/conf.h>

#include <gfx/convert.h>

static const u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };
static const u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff };


D_DEBUG_DOMAIN( Core_SurfBuffer, "Core/SurfBuffer", "DirectFB Core Surface Buffer" );

/**********************************************************************************************************************/

static void
surface_buffer_destructor( FusionObject *object, bool zombie, void *ctx )
{
     CoreSurfaceAllocation *allocation;
     unsigned int           i;
     CoreSurfaceBuffer     *buffer  = (CoreSurfaceBuffer*) object;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfBuffer, "destroying %p (%dx%d%s)\n", buffer,
                 buffer->config.size.w, buffer->config.size.h, zombie ? " ZOMBIE" : "");

     D_DEBUG_AT( Core_SurfBuffer, "  -> allocs %d\n", buffer->allocs.count );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     if (buffer->surface)
          dfb_surface_lock( buffer->surface );

     fusion_vector_foreach_reverse (allocation, i, buffer->allocs) {
          CORE_SURFACE_ALLOCATION_ASSERT( allocation );

          dfb_surface_allocation_decouple( allocation );
     }

     if (buffer->surface)
          dfb_surface_unlock( buffer->surface );

     fusion_vector_destroy( &buffer->allocs );

     direct_serial_deinit( &buffer->serial );

     D_MAGIC_CLEAR( buffer );

     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_surface_buffer_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Buffer Pool",
                                       sizeof(CoreSurfaceBuffer),
                                       sizeof(CoreSurfaceBufferNotification),
                                       surface_buffer_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_buffer_create( CoreDFB                 *core,
                           CoreSurface             *surface,
                           CoreSurfaceBufferFlags   flags,
                           int                      index,
                           CoreSurfaceBuffer      **ret_buffer )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_FLAGS_ASSERT( flags, CSBF_ALL );
     D_ASSERT( ret_buffer != NULL );

#if DIRECT_BUILD_DEBUG
     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_new( %s )\n", dfb_pixelformat_name( surface->config.format ) );

     if (flags & CSBF_STICKED)
          D_DEBUG_AT( Core_SurfBuffer, "  -> STICKED\n" );
#endif

     buffer = dfb_core_create_surface_buffer( core );
     if (!buffer)
          return DFB_FUSION;

     direct_serial_init( &buffer->serial );
     direct_serial_increase( &buffer->serial );

     buffer->surface     = surface;
     buffer->flags       = flags;
     buffer->format      = surface->config.format;
     buffer->config      = surface->config;
     buffer->type        = surface->type;
     buffer->resource_id = surface->resource_id;
     buffer->index       = index;

     if (surface->config.caps & DSCAPS_VIDEOONLY)
          buffer->policy = CSP_VIDEOONLY;
     else if (surface->config.caps & DSCAPS_SYSTEMONLY)
          buffer->policy = CSP_SYSTEMONLY;
     else
          buffer->policy = CSP_VIDEOLOW;

     fusion_vector_init( &buffer->allocs, 2, surface->shmpool );

     fusion_object_set_lock( &buffer->object, &surface->lock );

     fusion_ref_add_permissions( &buffer->object.ref, 0, FUSION_REF_PERMIT_REF_UNREF_LOCAL );

     D_MAGIC_SET( buffer, CoreSurfaceBuffer );

     *ret_buffer = buffer;

     if (surface->type & CSTF_PREALLOCATED) {
          CoreSurfacePool       *pool;
          CoreSurfaceAllocation *alloc;

          ret = dfb_surface_pools_lookup( surface->config.preallocated_pool_id, &pool );
          if (ret) {
               fusion_object_destroy( &buffer->object );
               return ret;
          }

          ret = dfb_surface_pool_allocate( pool, buffer, NULL, 0, &alloc );
          if (ret) {
               fusion_object_destroy( &buffer->object );
               return ret;
          }

          dfb_surface_allocation_update( alloc, CSAF_WRITE );
     }

     fusion_object_activate( &buffer->object );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_decouple( CoreSurfaceBuffer *buffer )
{
     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     dfb_surface_buffer_deallocate( buffer );

     buffer->surface = NULL;

     dfb_surface_buffer_unlink( &buffer );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_deallocate( CoreSurfaceBuffer *buffer )
{
     CoreSurfaceAllocation *allocation;
     int                    i;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p [%dx%d] )\n", __FUNCTION__,
                 buffer, buffer->config.size.w, buffer->config.size.h );

     fusion_vector_foreach_reverse (allocation, i, buffer->allocs) {
           CORE_SURFACE_ALLOCATION_ASSERT( allocation );

           dfb_surface_allocation_decouple( allocation );
     }

     return DFB_OK;
}

CoreSurfaceAllocation *
dfb_surface_buffer_find_allocation( CoreSurfaceBuffer       *buffer,
                                    CoreSurfaceAccessorID    accessor,
                                    CoreSurfaceAccessFlags   flags,
                                    bool                     lock )
{
     int                    i;
     CoreSurfaceAllocation *alloc;
     CoreSurfaceAllocation *uptodate = NULL;
     CoreSurfaceAllocation *outdated = NULL;

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );

     /*
      * For preallocated surfaces, when the client specified DSCAPS_STATIC_ALLOC,
      * it is forced to always get the same preallocated buffer again on each Lock.
      */
     if (buffer->type & CSTF_PREALLOCATED && buffer->config.caps & DSCAPS_STATIC_ALLOC) {
          D_MAGIC_ASSERT( buffer->surface, CoreSurface );

          if (buffer->surface->object.identity == Core_GetIdentity()) {
               D_DEBUG_AT( Core_SurfBuffer, "  -> DSCAPS_STATIC_ALLOC, returning preallocated buffer\n" );

               D_ASSERT( buffer->allocs.count > 0 );

               alloc = buffer->allocs.elements[0];

               D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );

               D_ASSERT( alloc->flags & CSALF_PREALLOCATED );

               /* Return if allocation has required flags. */
               if (D_FLAGS_ARE_SET( alloc->access[accessor], flags ))
                    return alloc;
          }
     }

     /* Prefer allocations which are up to date. */
     fusion_vector_foreach (alloc, i, buffer->allocs) {
          if (lock && alloc->flags & CSALF_PREALLOCATED) {
               if (!(alloc->access[accessor] & CSAF_SHARED)) {
                    D_DEBUG_AT( Core_SurfBuffer, "  -> non-shared preallocated buffer, surface identity %lu, core identity %lu\n",
                                buffer->surface->object.identity, Core_GetIdentity() );

                    /*
                     * If this is a non-shared preallocated allocation and the lock is not
                     * for the creator, we need to skip it and possibly allocate/update in
                     * a different pool.
                     */
                    if (buffer->surface->object.identity != Core_GetIdentity())
                         continue;
               }
          }

          if (Core_GetIdentity() != FUSION_ID_MASTER && !(alloc->access[accessor] & CSAF_SHARED)) {
               D_DEBUG_AT( Core_SurfBuffer, "    -> REFUSING ALLOCATION FOR SLAVE FROM NON-SHARED POOL!!!\n" );
               continue;
          }

          if (direct_serial_check( &alloc->serial, &buffer->serial )) {
               /* Return immediately if up to date allocation has required flags. */
               if (D_FLAGS_ARE_SET( alloc->access[accessor], flags ))
                    return alloc;

               /* Remember up to date allocation in case none has supported flags. */
               uptodate = alloc;
          }
          else if (D_FLAGS_ARE_SET( alloc->access[accessor], flags )) {
               /* Remember outdated allocation which has supported flags though. */
               outdated = alloc;
          }
     }

     /* In case of a lock the flags are mandatory and the outdated allocation has to be used... */
     if (lock)
          return outdated;

     /* ...otherwise we can still prefer the up to date allocation for Read/Write()! */
     return uptodate ?: outdated;
}

CoreSurfaceAllocation *
dfb_surface_buffer_find_allocation_key( CoreSurfaceBuffer *buffer,
                                        const char        *key )
{
     int                    i;
     CoreSurfaceAllocation *alloc;

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );

     fusion_vector_foreach (alloc, i, buffer->allocs) {
          if (dfb_surface_pool_check_key( alloc->pool, buffer, key, 0 ) == DFB_OK)
               return alloc;
     }

     return NULL;
}

DFBResult
dfb_surface_buffer_lock( CoreSurfaceBuffer      *buffer,
                         CoreSurfaceAccessorID   accessor,
                         CoreSurfaceAccessFlags  access,
                         CoreSurfaceBufferLock  *lock )
{
     DFBResult              ret;
     CoreSurface           *surface;
     CoreSurfaceAllocation *allocation = NULL;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_FLAGS_ASSERT( access, CSAF_ALL );
     D_ASSERT( lock != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_ASSERT( accessor >= CSAID_CPU );
     D_ASSUME( accessor < _CSAID_NUM );
     if (accessor >= CSAID_ANY) {
          D_UNIMPLEMENTED();
          return DFB_UNIMPLEMENTED;
     }

     if (accessor < 0 || accessor >= _CSAID_NUM)
          return DFB_INVARG;

#if DIRECT_BUILD_DEBUG
     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, 0x%02x, %p ) <- %dx%d %s [%d]\n", __FUNCTION__, buffer, access, lock,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(buffer->format),
                 dfb_surface_buffer_index(buffer) );

     switch (accessor) {
          case CSAID_CPU:
               D_DEBUG_AT( Core_SurfBuffer, "  -> CPU %s%s\n",
                           (access & CSAF_READ) ? "READ" : "", (access & CSAF_WRITE) ? "WRITE" : "" );
               break;

          case CSAID_GPU:
               D_DEBUG_AT( Core_SurfBuffer, "  -> GPU %s%s\n",
                           (access & CSAF_READ) ? "READ" : "", (access & CSAF_WRITE) ? "WRITE" : "" );
               break;

          case CSAID_LAYER0:
          case CSAID_LAYER1:
          case CSAID_LAYER2:
          case CSAID_LAYER3:
          case CSAID_LAYER4:
          case CSAID_LAYER5:
          case CSAID_LAYER6:
          case CSAID_LAYER7:
          case CSAID_LAYER8:
          case CSAID_LAYER9:
          case CSAID_LAYER10:
          case CSAID_LAYER11:
          case CSAID_LAYER12:
          case CSAID_LAYER13:
          case CSAID_LAYER14:
          case CSAID_LAYER15:
               D_DEBUG_AT( Core_SurfBuffer, "  -> LAYER %d %s%s\n", accessor - CSAID_LAYER0,
                           (access & CSAF_READ) ? "READ" : "", (access & CSAF_WRITE) ? "WRITE" : "" );
               break;

          default:
               D_DEBUG_AT( Core_SurfBuffer, "  -> other\n" );
               break;
     }

     if (access & CSAF_SHARED)
          D_DEBUG_AT( Core_SurfBuffer, "  -> SHARED\n" );
#endif

     D_DEBUG_AT( Core_SurfBuffer, "  -> Calling PreLockBuffer( buffer %p )...\n", buffer );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Run all code that modifies shared memory in master process (IPC call) */
     ret = CoreSurface_PreLockBuffer( surface, buffer, accessor, access, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_SurfBuffer, "  -> PreLockBuffer returned allocation %p (%s)\n", allocation, allocation->pool->desc.name );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( lock, accessor, access );


     ret = dfb_surface_pool_lock( allocation->pool, allocation, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                    allocation->pool->desc.name );
          dfb_surface_buffer_lock_deinit( lock );

          dfb_surface_allocation_unref( allocation );
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_unlock( CoreSurfaceBufferLock *lock )
{
     DFBResult              ret;
     CoreSurfacePool       *pool;
     CoreSurfaceAllocation *allocation;

     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_unlock( %p )\n", lock );

     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     allocation = lock->allocation;
     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     pool = allocation->pool;
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     /*
      * FIXME: This should fail with a nested GPU Lock during a CPU Lock and/or vice versa?
      */
//     D_ASSUME( D_FLAGS_ARE_SET( allocation->accessed, lock->access ) );

     ret = dfb_surface_pool_unlock( pool, lock->allocation, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Unlocking allocation failed! [%s]\n", pool->desc.name );
          return ret;
     }

     dfb_surface_buffer_lock_reset( lock );

     dfb_surface_buffer_lock_deinit( lock );

     dfb_surface_allocation_unref( allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_read( CoreSurfaceBuffer  *buffer,
                         void               *destination,
                         int                 pitch,
                         const DFBRectangle *prect )
{
     DFBResult              ret;
     int                    y;
     int                    bytes;
     DFBRectangle           rect;
     CoreSurface           *surface;
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
     if (fusion_vector_is_empty( &buffer->allocs )) {
          for (y=0; y<rect.h; y++) {
               memset( destination, 0, bytes );

               destination += pitch;
          }

          return DFB_OK;
     }

     D_DEBUG_AT( Core_SurfBuffer, "  -> Calling PreReadBuffer...\n" );

     /* Run all code that modifies shared memory in master process (IPC call) */
     ret = CoreSurface_PreReadBuffer( surface, buffer, &rect, &allocation );
     if (ret)
          return ret;

     D_DEBUG_AT( Core_SurfBuffer, "  -> PreReadBuffer returned allocation %p (%s)\n", allocation, allocation->pool->desc.name );

     /* Try reading from allocation directly... */
     ret = dfb_surface_pool_read( allocation->pool, allocation, destination, pitch, &rect );
     if (ret) {
          /* ...otherwise use fallback method via locking if possible. */
          if (allocation->access[CSAID_CPU] & CSAF_READ) {
               CoreSurfaceBufferLock lock;

               /* Lock the allocation. */
               dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_READ );

               ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                              allocation->pool->desc.name );
                    dfb_surface_buffer_lock_deinit( &lock );
                    dfb_surface_allocation_unref( allocation );
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

               dfb_surface_buffer_lock_deinit( &lock );
          }
     }

     dfb_surface_allocation_unref( allocation );

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
     CoreSurface           *surface;
     CoreSurfaceAllocation *allocation = NULL;

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

     if (prect) {
          if (!dfb_rectangle_intersect( &rect, prect )) {
               D_DEBUG_AT( Core_SurfBuffer, "  -> no intersection!\n" );
               return DFB_INVAREA;
          }
     
          if (!DFB_RECTANGLE_EQUAL( rect, *prect )) {
               D_DEBUG_AT( Core_SurfBuffer, "  -> got clipped to %d,%d-%dx%d!\n", DFB_RECTANGLE_VALS(&rect) );
               return DFB_INVAREA;
          }
     }

     D_DEBUG_AT( Core_SurfBuffer, "  -> %d,%d - %dx%d (%s)\n", DFB_RECTANGLE_VALS(&rect),
                 dfb_pixelformat_name( surface->config.format ) );

     D_DEBUG_AT( Core_SurfBuffer, "  -> Calling PreWriteBuffer...\n" );

     /* Run all code that modifies shared memory in master process (IPC call) */
     ret = CoreSurface_PreWriteBuffer( surface, buffer, &rect, &allocation );
     if (ret)
          return ret;

     D_DEBUG_AT( Core_SurfBuffer, "  -> PreWriteBuffer returned allocation %p (%s)\n", allocation, allocation->pool->desc.name );

     /* Try writing to allocation directly... */
     ret = source ? dfb_surface_pool_write( allocation->pool, allocation, source, pitch, &rect ) : DFB_UNSUPPORTED;
     if (ret) {
          /* ...otherwise use fallback method via locking if possible. */
          if (allocation->access[CSAID_CPU] & CSAF_WRITE) {
               int                   y;
               int                   bytes;
               DFBSurfacePixelFormat format;
               CoreSurfaceBufferLock lock;

               /* Calculate bytes per written line. */
               format = surface->config.format;
               bytes  = DFB_BYTES_PER_LINE( format, rect.w );

               /* Lock the allocation. */
               dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_WRITE );

               ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                              allocation->pool->desc.name );
                    dfb_surface_buffer_lock_deinit( &lock );
                    dfb_surface_allocation_unref( allocation );
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

               dfb_surface_buffer_lock_deinit( &lock );
          }
     }

     dfb_surface_allocation_unref( allocation );

     return ret;
}

DFBResult
dfb_surface_buffer_dump_type_locked( CoreSurfaceBuffer     *buffer,
                                     const char            *directory,
                                     const char            *prefix,
                                     bool                   raw,
                                     CoreSurfaceBufferLock *lock )
{
     int                num  = -1;
     int                fd_p = -1;
     int                fd_g = -1;
     int                i, n;
     int                len = (directory ? strlen(directory) : 0) + (prefix ? strlen(prefix) : 0) + 40;
     char               filename[len];
     char               head[30];
     bool               rgb   = false;
     bool               alpha = false;
#ifdef USE_ZLIB
     gzFile             gz_p = NULL, gz_g = NULL;
     static const char *gz_ext = ".gz";
#else
     static const char *gz_ext = "";
     int                res;
#endif
     char               rgb_ext[4];
     CoreSurface       *surface;
     CorePalette       *palette = NULL;

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p, %p )\n", __FUNCTION__, buffer, directory, prefix );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( directory != NULL );

     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );
     CORE_SURFACE_ALLOCATION_ASSERT( lock->allocation );

     surface = buffer->surface;
     CORE_SURFACE_ASSERT( surface );

     /* Check pixel format. */
     switch (buffer->format) {
          case DSPF_LUT8:
          case DSPF_ALUT8:
               palette = surface->palette;

               if (!palette) {
                    D_BUG( "no palette" );
                    return DFB_BUG;
               }

               if (dfb_palette_ref( palette ))
                    return DFB_FUSION;

               rgb = true;

               /* fall through */

          case DSPF_A8:
               alpha = true;
               break;

          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_ARGB1555:
          case DSPF_RGBA5551:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_AiRGB:
          case DSPF_ARGB8565:
          case DSPF_AYUV:
          case DSPF_AVYU:
               alpha = true;

               /* fall through */

          case DSPF_RGB332:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_NV16:
          case DSPF_YV16:
          case DSPF_RGB444:
          case DSPF_RGB555:
          case DSPF_BGR555:
          case DSPF_YUV444P:
          case DSPF_VYU:
               rgb   = true;
               break;


          default:
               D_ERROR( "DirectFB/core/surfaces: surface dump for format "
                         "'%s' is not implemented!\n",
                        dfb_pixelformat_name( buffer->format ) );
               return DFB_UNSUPPORTED;
     }

     /* Setup the file extension depending on whether we want the output the RAW format or not... */
     snprintf( rgb_ext, D_ARRAY_SIZE(rgb_ext), (raw == true) ? "raw" : "ppm");

     if (prefix) {
          /* Find the lowest unused index. */
          while (++num < 10000) {
               snprintf( filename, len, "%s/%s_%04d.%s%s",
                         directory, prefix, num, rgb_ext, gz_ext );

               if (access( filename, F_OK ) != 0) {
                    snprintf( filename, len, "%s/%s_%04d.pgm%s",
                              directory, prefix, num, gz_ext );

                    if (access( filename, F_OK ) != 0)
                         break;
               }
          }

          if (num == 10000) {
               D_ERROR( "DirectFB/core/surfaces: "
                        "couldn't find an unused index for surface dump!\n" );
               if (palette)
                    dfb_palette_unref( palette );
               return DFB_FAILURE;
          }
     }

     /* Create a file with the found index. */
     if (rgb) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.%s%s", directory, prefix, num, rgb_ext, gz_ext );
          else
               snprintf( filename, len, "%s.%s%s", directory, rgb_ext, gz_ext );

          fd_p = open( filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (fd_p < 0) {
               D_PERROR("DirectFB/core/surfaces: "
                        "could not open %s!\n", filename);
               if (palette)
                    dfb_palette_unref( palette );
               return DFB_IO;
          }
     }

     /* Create a graymap for the alpha channel using the found index. */
     if (alpha && !raw) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.pgm%s", directory, prefix, num, gz_ext );
          else
               snprintf( filename, len, "%s.pgm%s", directory, gz_ext );

          fd_g = open( filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (fd_g < 0) {
               D_PERROR("DirectFB/core/surfaces: "
                         "could not open %s!\n", filename);

               if (palette)
                    dfb_palette_unref( palette );

               if (rgb) {
                    close( fd_p );
                    snprintf( filename, len, "%s/%s_%04d.%s%s",
                              directory, prefix, num, rgb_ext, gz_ext );
                    unlink( filename );
               }

               return DFB_IO;
          }
     }

#ifdef USE_ZLIB
     if (rgb)
          gz_p = gzdopen( fd_p, "wb" );

     if (alpha && !raw)
          gz_g = gzdopen( fd_g, "wb" );
#endif

     /* Only write the header if we are not dumping a raw image */
     if (!raw) {
     if (rgb) {
          /* Write the pixmap header. */
          snprintf( head, 30,
                    "P6\n%d %d\n255\n", surface->config.size.w, surface->config.size.h );
#ifdef USE_ZLIB
          gzwrite( gz_p, head, strlen(head) );
#else
          res = write( fd_p, head, strlen(head) );
          (void)res;
#endif
     }

     /* Write the graymap header. */
     if (alpha) {
          snprintf( head, 30,
                    "P5\n%d %d\n255\n", surface->config.size.w, surface->config.size.h );
#ifdef USE_ZLIB
          gzwrite( gz_g, head, strlen(head) );
#else
          res = write( fd_g, head, strlen(head) );
          (void)res;
#endif
	 }
     }

     /* Write the pixmap (and graymap) data. */
     for (i=0; i<surface->config.size.h; i++) {
          int n3;

          /* Prepare one row. */
          u8 *srces[3];
          int pitches[3];
          u8 *src8;

          dfb_surface_get_data_offsets( &surface->config, lock->addr, lock->pitch, 0, i,
                                        3, srces, pitches );
          src8 = srces[0];

          /* Write color buffer to pixmap file. */
          if (rgb) {
               if (raw) {
                   u8 buf_p[surface->config.size.w * 4];

                   if (buffer->format == DSPF_LUT8) {
                        for (n=0, n3=0; n<surface->config.size.w; n++, n3+=4) {
                             buf_p[n3+0] = palette->entries[src8[n]].r;
                             buf_p[n3+1] = palette->entries[src8[n]].g;
                             buf_p[n3+2] = palette->entries[src8[n]].b;
                             buf_p[n3+3] = palette->entries[src8[n]].a;
                        }
                   }
                   else
                        dfb_convert_to_argb( buffer->format, srces[0], pitches[0], surface->config.size.h,
                                             (u32 *)(&buf_p[0]), surface->config.size.w * 4, surface->config.size.w, 1 );
#ifdef USE_ZLIB
                   gzwrite( gz_p, buf_p, surface->config.size.w * 4 );
#else
                   write( fd_p, buf_p, surface->config.size.w * 4 );
#endif

               }
               else {
               u8 buf_p[surface->config.size.w * 3];

               if (buffer->format == DSPF_LUT8) {
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = palette->entries[src8[n]].r;
                         buf_p[n3+1] = palette->entries[src8[n]].g;
                         buf_p[n3+2] = palette->entries[src8[n]].b;
                    }
               }
               else
                    dfb_convert_to_rgb24( buffer->format, srces[0], pitches[0], surface->config.size.h,
                                          buf_p, surface->config.size.w * 3, surface->config.size.w, 1 );
#ifdef USE_ZLIB
               gzwrite( gz_p, buf_p, surface->config.size.w * 3 );
#else
               res = write( fd_p, buf_p, surface->config.size.w * 3 );
               (void)res;
#endif
          }
          }

          /* Write alpha buffer to graymap file. */
          if (alpha && !raw) {
               u8 buf_g[surface->config.size.w];

               if (buffer->format == DSPF_LUT8) {
                    for (n=0; n<surface->config.size.w; n++)
                         buf_g[n] = palette->entries[src8[n]].a;
               }
               else
                    dfb_convert_to_a8( buffer->format, srces[0], pitches[0], surface->config.size.h,
                                       buf_g, surface->config.size.w, surface->config.size.w, 1 );
#ifdef USE_ZLIB
               gzwrite( gz_g, buf_g, surface->config.size.w );
#else
               res = write( fd_g, buf_g, surface->config.size.w );
               (void)res;
#endif
          }
     }

     /* Release the palette. */
     if (palette)
          dfb_palette_unref( palette );

#ifdef USE_ZLIB
     if (rgb)
          gzclose( gz_p );

     if (alpha && !raw)
          gzclose( gz_g );
#endif

     /* Close pixmap file. */
     if (rgb)
          close( fd_p );

     /* Close graymap file. */
     if (alpha && !raw)
          close( fd_g );

     return DFB_OK;
}

static DFBResult
dfb_surface_buffer_dump_type( CoreSurfaceBuffer *buffer,
                              const char        *directory,
                              const char        *prefix,
                              bool               raw )
{
     DFBResult             ret;
     CoreSurfaceBufferLock lock;

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p, %p )\n", __FUNCTION__, buffer, directory, prefix );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( directory != NULL );

     /* Lock the surface buffer, get the data pointer and pitch. */
     ret = dfb_surface_buffer_lock( buffer, CSAID_CPU, CSAF_READ, &lock );
     if (ret)
          return ret;

     ret = dfb_surface_buffer_dump_type_locked( buffer, directory, prefix, raw, &lock );

     /* Unlock the surface buffer. */
     dfb_surface_buffer_unlock( &lock );

     return ret;
}

DFBResult
dfb_surface_buffer_dump( CoreSurfaceBuffer *buffer,
                         const char        *directory,
                         const char        *prefix )
{
     return dfb_surface_buffer_dump_type( buffer, directory, prefix, false );
}

DFBResult
dfb_surface_buffer_dump_raw( CoreSurfaceBuffer *buffer,
                             const char        *directory,
                             const char        *prefix )
{
     return dfb_surface_buffer_dump_type( buffer, directory, prefix, true );
}

/**********************************************************************************************************************/
