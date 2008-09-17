/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>

#include <misc/conf.h>

#include <gfx/convert.h>

static const u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };
static const u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff };


D_DEBUG_DOMAIN( Core_SurfBuffer, "Core/SurfBuffer", "DirectFB Core Surface Buffer" );

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
     FUSION_SKIRMISH_ASSERT( &surface->lock );

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
                         CoreSurfaceAccessorID   accessor,
                         CoreSurfaceAccessFlags  access,
                         CoreSurfaceBufferLock  *lock )
{
     DFBResult              ret;
     int                    i;
     CoreSurface           *surface;
     CoreSurfaceAllocation *alloc      = NULL;
     CoreSurfaceAllocation *allocation = NULL;
     bool                   allocated  = false;

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
     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_lock( %p, 0x%02x, %p ) <- %dx%d %s [%d]\n", buffer, access, lock,
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

     fusion_vector_foreach (alloc, i, buffer->allocs) {
          CORE_SURFACE_ALLOCATION_ASSERT( alloc );

          if (D_FLAGS_ARE_SET( alloc->access[accessor], access )) {
               /* Take last up to date or first available. */
               if (!allocation || direct_serial_check( &alloc->serial, &buffer->serial ))
                    allocation = alloc;
               //break;
          }
     }

     if (!allocation) {
          D_DEBUG_AT( Core_SurfBuffer, "  -> no suitable allocation (yet)!\n" );

          ret = dfb_surface_pools_allocate( buffer, accessor, access, &allocation );
          if (ret)
               return ret;

          CORE_SURFACE_ALLOCATION_ASSERT( allocation );

          allocated = true;
     }

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, access );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_pool_deallocate( allocation->pool, allocation );
          return ret;
     }

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( lock, accessor, access );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                    allocation->pool->desc.name );
          D_MAGIC_CLEAR( lock );

          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_pool_deallocate( allocation->pool, allocation );

          return ret;
     }

     lock->allocation = allocation;


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
                    allocation->accessed[CSAID_GPU] &= ~CSAF_WRITE;

                    /* ...clear hardware read access (to avoid syncing twice). */
                    allocation->accessed[CSAID_GPU] &= ~CSAF_READ;
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
                         allocation->accessed[CSAID_GPU] &= ~CSAF_READ;
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
                    allocation->accessed[CSAID_CPU] &= ~CSAF_WRITE;
          }
     }

     if (! D_FLAGS_ARE_SET( allocation->accessed[accessor], access )) {
          /* FIXME: surface_enter */
     }

     /* Collect... */
     allocation->accessed[accessor] |= access;

     /* FIXME: don't use weak counter */
     buffer->locked++;

     D_DEBUG_AT( Core_SurfBuffer, "  -> locked %dx now\n", buffer->locked );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_unlock( CoreSurfaceBufferLock *lock )
{
     DFBResult              ret;
     CoreSurfacePool       *pool;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;

     D_DEBUG_AT( Core_SurfBuffer, "dfb_surface_buffer_unlock( %p )\n", lock );

     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_MAGIC_ASSERT( lock->buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( lock->buffer->surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &lock->buffer->surface->lock );

     allocation = lock->allocation;
     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     buffer = lock->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

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

     buffer->locked--;

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
     if (fusion_vector_is_empty( &buffer->allocs )) {
          for (y=0; y<rect.h; y++) {
               memset( destination, 0, bytes );

               destination += pitch;
          }

          return DFB_OK;
     }

     /* Look for allocation with CPU access. */
     fusion_vector_foreach (alloc, i, buffer->allocs) {
          if (D_FLAGS_ARE_SET( alloc->access[CSAID_CPU], CSAF_READ )) {
               allocation = alloc;
               break;
          }
     }

     /* FIXME: use Read() */
     if (!allocation)
          return DFB_UNIMPLEMENTED;

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, CSAF_READ );
     if (ret)
          return ret;

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_READ );

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

static CoreSurfaceAllocation *
find_allocation( CoreSurfaceBuffer       *buffer,
                 CoreSurfaceAccessFlags   flags,
                 bool                     lock )
{
     int                    i;
     CoreSurfaceAllocation *alloc;
     CoreSurfaceAllocation *uptodate = NULL;
     CoreSurfaceAllocation *outdated = NULL;

     /* Prefer allocations which are up to date. */
     fusion_vector_foreach (alloc, i, buffer->allocs) {
          if (direct_serial_check( &alloc->serial, &buffer->serial )) {
               /* Return immediately if up to date allocation has required flags. */
               if (D_FLAGS_ARE_SET( alloc->access[CSAID_CPU], flags ))
                    return alloc;

               /* Remember up to date allocation in case none has supported flags. */
               uptodate = alloc;
          }
          else if (D_FLAGS_ARE_SET( alloc->access[CSAID_CPU], flags )) {
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
     bool                   allocated  = false;

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

     D_DEBUG_AT( Core_SurfBuffer, "  -> %d,%d - %dx%d (%s)\n", DFB_RECTANGLE_VALS(&rect),
                 dfb_pixelformat_name( surface->config.format ) );

     /* Use last read allocation if it's up to date... */
     if (buffer->read && direct_serial_check( &buffer->read->serial, &buffer->serial ))
          allocation = buffer->read;
     else {
          /* ...otherwise look for allocation with CPU access. */
          allocation = find_allocation( buffer, CSAF_WRITE, false );
          if (!allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( buffer, CSAID_CPU, CSAF_WRITE, &allocation );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Buffer allocation failed!\n" );
                    return ret;
               }

               allocated = true;
          }
     }

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, CSAF_WRITE );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_pool_deallocate( allocation->pool, allocation );
          return ret;
     }

     /* Try writing to allocation directly... */
     ret = dfb_surface_pool_write( allocation->pool, allocation, source, pitch, &rect );
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
          }
     }

     return ret;
}

DFBResult
dfb_surface_buffer_dump( CoreSurfaceBuffer *buffer,
                         const char        *directory,
                         const char        *prefix )
{
     DFBResult              ret;
     int                    num  = -1;
     int                    fd_p = -1;
     int                    fd_g = -1;
     int                    i, n;
     int                    len = (directory ? strlen(directory) : 0) + (prefix ? strlen(prefix) : 0) + 40;
     char                   filename[len];
     char                   head[30];
     bool                   rgb   = false;
     bool                   alpha = false;
#ifdef USE_ZLIB
     gzFile                 gz_p = NULL, gz_g = NULL;
     static const char     *gz_ext = ".gz";
#else
     static const char     *gz_ext = "";
#endif
     CoreSurface           *surface;
     CorePalette           *palette = NULL;
     CoreSurfaceBufferLock  lock;

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p, %p )\n", __FUNCTION__, buffer, directory, prefix );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( directory != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     /* Check pixel format. */
     switch (buffer->format) {
          case DSPF_LUT8:
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
          case DSPF_ARGB1555:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_AiRGB:
               alpha = true;

               /* fall through */

          case DSPF_RGB332:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_NV16:
          case DSPF_RGB444:
          case DSPF_RGB555:
          case DSPF_BGR555:
               rgb   = true;
               break;


          default:
               D_ERROR( "DirectFB/core/surfaces: surface dump for format "
                         "'%s' is not implemented!\n",
                        dfb_pixelformat_name( buffer->format ) );
               return DFB_UNSUPPORTED;
     }

     /* Lock the surface buffer, get the data pointer and pitch. */
     ret = dfb_surface_buffer_lock( buffer, CSAID_CPU, CSAF_READ, &lock );
     if (ret) {
          if (palette)
               dfb_palette_unref( palette );
          return ret;
     }

     if (prefix) {
          /* Find the lowest unused index. */
          while (++num < 10000) {
               snprintf( filename, len, "%s/%s_%04d.ppm%s",
                         directory, prefix, num, gz_ext );

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
               dfb_surface_buffer_unlock( &lock );
               if (palette)
                    dfb_palette_unref( palette );
               return DFB_FAILURE;
          }
     }

     /* Create a file with the found index. */
     if (rgb) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.ppm%s", directory, prefix, num, gz_ext );
          else
               snprintf( filename, len, "%s.ppm%s", directory, gz_ext );

          fd_p = open( filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (fd_p < 0) {
               D_PERROR("DirectFB/core/surfaces: "
                        "could not open %s!\n", filename);
               dfb_surface_buffer_unlock( &lock );
               if (palette)
                    dfb_palette_unref( palette );
               return DFB_IO;
          }
     }

     /* Create a graymap for the alpha channel using the found index. */
     if (alpha) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.pgm%s", directory, prefix, num, gz_ext );
          else
               snprintf( filename, len, "%s.pgm%s", directory, gz_ext );

          fd_g = open( filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (fd_g < 0) {
               D_PERROR("DirectFB/core/surfaces: "
                         "could not open %s!\n", filename);

               dfb_surface_buffer_unlock( &lock );
               if (palette)
                    dfb_palette_unref( palette );

               if (rgb) {
                    close( fd_p );
                    snprintf( filename, len, "%s/%s_%04d.ppm%s",
                              directory, prefix, num, gz_ext );
                    unlink( filename );
               }

               return DFB_IO;
          }
     }

#ifdef USE_ZLIB
     if (rgb)
          gz_p = gzdopen( fd_p, "wb" );

     if (alpha)
          gz_g = gzdopen( fd_g, "wb" );
#endif

     if (rgb) {
          /* Write the pixmap header. */
          snprintf( head, 30,
                    "P6\n%d %d\n255\n", surface->config.size.w, surface->config.size.h );
#ifdef USE_ZLIB
          gzwrite( gz_p, head, strlen(head) );
#else
          write( fd_p, head, strlen(head) );
#endif
     }

     /* Write the graymap header. */
     if (alpha) {
          snprintf( head, 30,
                    "P5\n%d %d\n255\n", surface->config.size.w, surface->config.size.h );
#ifdef USE_ZLIB
          gzwrite( gz_g, head, strlen(head) );
#else
          write( fd_g, head, strlen(head) );
#endif
     }

     /* Write the pixmap (and graymap) data. */
     for (i=0; i<surface->config.size.h; i++) {
          int    n3;
          u8    *data8;
          u16 *data16;
          u32 *data32;

          u8 buf_p[surface->config.size.w * 3];
          u8 buf_g[surface->config.size.w];

          /* Prepare one row. */
          data8  = dfb_surface_data_offset( surface, lock.addr, lock.pitch, 0, i );
          data16 = (u16*) data8;
          data32 = (u32*) data8;

          switch (buffer->format) {
               case DSPF_LUT8:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = palette->entries[data8[n]].r;
                         buf_p[n3+1] = palette->entries[data8[n]].g;
                         buf_p[n3+2] = palette->entries[data8[n]].b;

                         buf_g[n] = palette->entries[data8[n]].a;
                    }
                    break;
               case DSPF_A8:
                    direct_memcpy( &buf_g[0], data8, surface->config.size.w );
                    break;
               case DSPF_AiRGB:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data32[n] & 0xFF0000) >> 16;
                         buf_p[n3+1] = (data32[n] & 0x00FF00) >>  8;
                         buf_p[n3+2] = (data32[n] & 0x0000FF);

                         buf_g[n] = ~(data32[n] >> 24);
                    }
                    break;
               case DSPF_ARGB:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data32[n] & 0xFF0000) >> 16;
                         buf_p[n3+1] = (data32[n] & 0x00FF00) >>  8;
                         buf_p[n3+2] = (data32[n] & 0x0000FF);

                         buf_g[n] = data32[n] >> 24;
                    }
                    break;
               case DSPF_ARGB1555:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0x7C00) >> 7;
                         buf_p[n3+1] = (data16[n] & 0x03E0) >> 2;
                         buf_p[n3+2] = (data16[n] & 0x001F) << 3;

                         buf_g[n] = (data16[n] & 0x8000) ? 0xff : 0x00;
                    }
                    break;
               case DSPF_RGB555:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0x7C00) >> 7;
                         buf_p[n3+1] = (data16[n] & 0x03E0) >> 2;
                         buf_p[n3+2] = (data16[n] & 0x001F) << 3;
                    }
                    break;

               case DSPF_BGR555:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+2] = (data16[n] & 0x7C00) >> 7;
                         buf_p[n3+1] = (data16[n] & 0x03E0) >> 2;
                         buf_p[n3+0] = (data16[n] & 0x001F) << 3;
                    }
                    break;

               case DSPF_ARGB2554:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0x3E00) >> 6;
                         buf_p[n3+1] = (data16[n] & 0x01F0) >> 1;
                         buf_p[n3+2] = (data16[n] & 0x000F) << 4;

                         switch (data16[n] >> 14) {
                              case 0:
                                   buf_g[n] = 0x00;
                                   break;
                              case 1:
                                   buf_g[n] = 0x55;
                                   break;
                              case 2:
                                   buf_g[n] = 0xAA;
                                   break;
                              case 3:
                                   buf_g[n] = 0xFF;
                                   break;
                         }
                    }
                    break;
               case DSPF_ARGB4444:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0x0F00) >> 4;
                         buf_p[n3+1] = (data16[n] & 0x00F0);
                         buf_p[n3+2] = (data16[n] & 0x000F) << 4;

                         buf_g[n]  = (data16[n] >> 12);
                         buf_g[n] |= buf_g[n] << 4;
                    }
                    break;
              case DSPF_RGB444:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0x0F00) >> 4;
                         buf_p[n3+1] = (data16[n] & 0x00F0);
                         buf_p[n3+2] = (data16[n] & 0x000F) << 4;
                    }
                    break;
               case DSPF_RGB332:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = lookup3to8[ (data8[n] >> 5)        ];
                         buf_p[n3+1] = lookup3to8[ (data8[n] >> 2) & 0x07 ];
                         buf_p[n3+2] = lookup2to8[ (data8[n]     ) & 0x03 ];
                    }
                    break;
               case DSPF_RGB16:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0xF800) >> 8;
                         buf_p[n3+1] = (data16[n] & 0x07E0) >> 3;
                         buf_p[n3+2] = (data16[n] & 0x001F) << 3;
                    }
                    break;
               case DSPF_RGB24:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
#ifdef WORDS_BIGENDIAN
                         buf_p[n3+0] = data8[n3+0];
                         buf_p[n3+1] = data8[n3+1];
                         buf_p[n3+2] = data8[n3+2];
#else
                         buf_p[n3+0] = data8[n3+2];
                         buf_p[n3+1] = data8[n3+1];
                         buf_p[n3+2] = data8[n3+0];
#endif
                    }
                    break;
               case DSPF_RGB32:
                    for (n=0, n3=0; n<surface->config.size.w; n++, n3+=3) {
                         buf_p[n3+0] = (data32[n] & 0xFF0000) >> 16;
                         buf_p[n3+1] = (data32[n] & 0x00FF00) >>  8;
                         buf_p[n3+2] = (data32[n] & 0x0000FF);
                    }
                    break;
               case DSPF_YUY2:
                    for (n=0, n3=0; n<surface->config.size.w/2; n++, n3+=6) {
                         register u32 y0, cb, y1, cr;
                         y0 = (data32[n] & 0x000000FF);
                         cb = (data32[n] & 0x0000FF00) >>  8;
                         y1 = (data32[n] & 0x00FF0000) >> 16;
                         cr = (data32[n] & 0xFF000000) >> 24;
                         YCBCR_TO_RGB( y0, cb, cr,
                                       buf_p[n3+0], buf_p[n3+1], buf_p[n3+2] );
                         YCBCR_TO_RGB( y1, cb, cr,
                                       buf_p[n3+3], buf_p[n3+4], buf_p[n3+5] );
                    }
                    break;
               case DSPF_UYVY:
                    for (n=0, n3=0; n<surface->config.size.w/2; n++, n3+=6) {
                         register u32 y0, cb, y1, cr;
                         cb = (data32[n] & 0x000000FF);
                         y0 = (data32[n] & 0x0000FF00) >>  8;
                         cr = (data32[n] & 0x00FF0000) >> 16;
                         y1 = (data32[n] & 0xFF000000) >> 24;
                         YCBCR_TO_RGB( y0, cb, cr,
                                       buf_p[n3+0], buf_p[n3+1], buf_p[n3+2] );
                         YCBCR_TO_RGB( y1, cb, cr,
                                       buf_p[n3+3], buf_p[n3+4], buf_p[n3+5] );
                    }
                    break;
               case DSPF_NV16: {
                    u16 *cbcr = (u16*)(data8 + surface->config.size.h * lock.pitch);

                    for (n=0, n3=0; n<surface->config.size.w/2; n++, n3+=6) {
#ifdef WORDS_BIGENDIAN
                         YCBCR_TO_RGB( data8[n*2+0], cbcr[n] >> 8, cbcr[n] & 0xff,
                                       buf_p[n3+0], buf_p[n3+1], buf_p[n3+2] );

                         YCBCR_TO_RGB( data8[n*2+1], cbcr[n] >> 8, cbcr[n] & 0xff,
                                       buf_p[n3+3], buf_p[n3+4], buf_p[n3+5] );
#else
                         YCBCR_TO_RGB( data8[n*2+0], cbcr[n] & 0xff, cbcr[n] >> 8,
                                       buf_p[n3+0], buf_p[n3+1], buf_p[n3+2] );

                         YCBCR_TO_RGB( data8[n*2+1], cbcr[n] & 0xff, cbcr[n] >> 8,
                                       buf_p[n3+3], buf_p[n3+4], buf_p[n3+5] );
#endif
                    }
                    break;
               }
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }

          /* Write color buffer to pixmap file. */
          if (rgb)
#ifdef USE_ZLIB
               gzwrite( gz_p, buf_p, surface->config.size.w * 3 );
#else
               write( fd_p, buf_p, surface->config.size.w * 3 );
#endif

          /* Write alpha buffer to graymap file. */
          if (alpha)
#ifdef USE_ZLIB
               gzwrite( gz_g, buf_g, surface->config.size.w );
#else
               write( fd_g, buf_g, surface->config.size.w );
#endif
     }

     /* Unlock the surface buffer. */
     dfb_surface_buffer_unlock( &lock );

     /* Release the palette. */
     if (palette)
          dfb_palette_unref( palette );

#ifdef USE_ZLIB
     if (rgb)
          gzclose( gz_p );

     if (alpha)
          gzclose( gz_g );
#endif

     /* Close pixmap file. */
     if (rgb)
          close( fd_p );

     /* Close graymap file. */
     if (alpha)
          close( fd_g );

     return DFB_OK;
}

/**********************************************************************************************************************/

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
allocation_update_copy( CoreSurfaceAllocation *allocation,
                        CoreSurfaceAllocation *source )
{
     DFBResult              ret;
     CoreSurfaceBufferLock  src;
     CoreSurfaceBufferLock  dst;
     CoreSurfaceBuffer     *buffer;

     D_DEBUG_AT( Core_SurfBuffer, "%s()\n", __FUNCTION__ );

     D_ASSERT( allocation != source );

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( source, CoreSurfaceAllocation );

     D_ASSERT( source->buffer == allocation->buffer );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Lock the source allocation. */
     dfb_surface_buffer_lock_init( &src, CSAID_CPU, CSAF_READ );

     ret = dfb_surface_pool_lock( source->pool, source, &src );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Could not lock source for transfer!\n" );
          D_MAGIC_CLEAR( &src );
          return ret;
     }

     /* Lock the destination allocation. */
     dfb_surface_buffer_lock_init( &dst, CSAID_CPU, CSAF_WRITE );

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

     return DFB_OK;
}

static DFBResult
allocation_update_write( CoreSurfaceAllocation *allocation,
                         CoreSurfaceAllocation *source )
{
     DFBResult              ret;
     CoreSurfaceBufferLock  src;
     CoreSurfaceBuffer     *buffer;

     D_DEBUG_AT( Core_SurfBuffer, "%s()\n", __FUNCTION__ );

     D_ASSERT( allocation != source );

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( source, CoreSurfaceAllocation );

     D_ASSERT( source->buffer == allocation->buffer );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Lock the source allocation. */
     dfb_surface_buffer_lock_init( &src, CSAID_CPU, CSAF_READ );

     ret = dfb_surface_pool_lock( source->pool, source, &src );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Could not lock source for transfer!\n" );
          D_MAGIC_CLEAR( &src );
          return ret;
     }

     /* Write to the destination allocation. */
     ret = dfb_surface_pool_write( allocation->pool, allocation, src.addr, src.pitch, NULL );
     if (ret)
          D_DERROR( ret, "Core/SurfBuffer: Could not write from destination allocation!\n" );

     dfb_surface_pool_unlock( source->pool, source, &src );

     D_MAGIC_CLEAR( &src );

     return ret;
}

static DFBResult
allocation_update_read( CoreSurfaceAllocation *allocation,
                        CoreSurfaceAllocation *source )
{
     DFBResult              ret;
     CoreSurfaceBufferLock  dst;
     CoreSurfaceBuffer     *buffer;

     D_DEBUG_AT( Core_SurfBuffer, "%s()\n", __FUNCTION__ );

     D_ASSERT( allocation != source );

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( source, CoreSurfaceAllocation );

     D_ASSERT( source->buffer == allocation->buffer );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     /* Lock the destination allocation. */
     dfb_surface_buffer_lock_init( &dst, CSAID_CPU, CSAF_WRITE );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &dst );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Could not lock destination for transfer!\n" );
          D_MAGIC_CLEAR( &dst );
          return ret;
     }

     /* Read from the source allocation. */
     ret = dfb_surface_pool_read( source->pool, source, dst.addr, dst.pitch, NULL );
     if (ret)
          D_DERROR( ret, "Core/SurfBuffer: Could not read from source allocation!\n" );

     dfb_surface_pool_unlock( allocation->pool, allocation, &dst );

     D_MAGIC_CLEAR( &dst );

     return ret;
}

DFBResult
dfb_surface_allocation_update( CoreSurfaceAllocation  *allocation,
                               CoreSurfaceAccessFlags  access )
{
     DFBResult              ret;
     int                    i;
     CoreSurfaceAllocation *alloc;
     CoreSurfaceBuffer     *buffer;

     D_DEBUG_AT( Core_SurfBuffer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_FLAGS_ASSERT( access, CSAF_ALL );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     if (direct_serial_update( &allocation->serial, &buffer->serial ) && buffer->written) {
          CoreSurfaceAllocation *source = buffer->written;

          D_ASSUME( allocation != source );

          D_DEBUG_AT( Core_SurfBuffer, "  -> updating allocation...\n" );

          D_MAGIC_ASSERT( source, CoreSurfaceAllocation );
          D_ASSERT( source->buffer == allocation->buffer );

          if ((source->access[CSAID_CPU] & CSAF_READ) && (allocation->access[CSAID_CPU] & CSAF_WRITE))
               ret = allocation_update_copy( allocation, source );
          else if (source->access[CSAID_CPU] & CSAF_READ)
               ret = allocation_update_write( allocation, source );
          else if (allocation->access[CSAID_CPU] & CSAF_WRITE)
               ret = allocation_update_read( allocation, source );
          else {
               D_UNIMPLEMENTED();
               ret = DFB_UNSUPPORTED;
          }

          if (ret) {
               D_DERROR( ret, "Core/SurfaceBuffer: Updating allocation failed!\n" );
               return ret;
          }
     }

     if (access & CSAF_WRITE) {
          D_DEBUG_AT( Core_SurfBuffer, "  -> increasing serial...\n" );

          direct_serial_increase( &buffer->serial );

          direct_serial_copy( &allocation->serial, &buffer->serial );

          buffer->written = allocation;
          buffer->read    = NULL;

          /* Zap volatile allocations (freed when no longer up to date). */
          fusion_vector_foreach (alloc, i, buffer->allocs) {
               D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );

               if (alloc != allocation && (alloc->flags & CSALF_VOLATILE)) {
                    dfb_surface_pool_deallocate( alloc->pool, alloc );
                    i--;
               }
          }
     }
     else
          buffer->read = allocation;

     /* Zap all other allocations? */
     if (dfb_config->thrifty_surface_buffers) {
          buffer->written = buffer->read = allocation;

          fusion_vector_foreach (alloc, i, buffer->allocs) {
               D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );

               /* Don't zap preallocated which would not really free up memory, but just loose the handle. */
               if (alloc != allocation && !(alloc->flags & (CSALF_PREALLOCATED | CSALF_MUCKOUT))) {
                    dfb_surface_pool_deallocate( alloc->pool, alloc );
                    i--;
               }
          }
     }

     return DFB_OK;
}

