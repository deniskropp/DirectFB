/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include <pthread.h>

#include <direct/list.h>
#include <fusion/shmalloc.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>
#include <core/system.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>


D_DEBUG_DOMAIN( Core_SM, "Core/SurfaceMgr", "DirectFB Surface Manager" );

typedef struct _Heap Heap;

/*
 * initially there is one big free chunk,
 * chunks are splitted into a free and an occupied chunk if memory is allocated,
 * two chunks are merged to one free chunk if memory is deallocated
 */
struct _Chunk {
     int                  magic;

     int                  offset;      /* offset in memory,
                                          is greater or equal to the heap offset */
     int                  length;      /* length of this chunk in bytes */
     
     SurfaceBuffer       *buffer;      /* pointer to surface buffer occupying
                                          this chunk, or NULL if chunk is free */

     int                  tolerations; /* number of times this chunk was scanned
                                          occupied, resetted in assure_video */

     Heap                *heap;        /* pointer to heap this chunk belongs to */

     Chunk               *prev;
     Chunk               *next;
};

struct _Heap {
     int                  magic;
     
     Chunk               *chunks;

     CoreSurfaceStorage   storage;        /* storage of the heap 
                                             (VIDEO or AUXILIARY) */
     
     int                  offset;
     int                  length;         /* length of the heap in bytes */
     int                  avail;          /* amount of available memory in bytes */

     int                  min_toleration;
     
     Heap                *prev;
     Heap                *next;
} SurfaceHeap;

struct _SurfaceManager {
     int                  magic;

     FusionSkirmish       lock;

     FusionSHMPoolShared *shmpool;

     Heap                *heaps;
     
     bool                 suspended;

     /* card limitations for surface offsets and their pitch */
     unsigned int         byteoffset_align;
     unsigned int         pixelpitch_align;
     unsigned int         bytepitch_align;

     unsigned int         max_power_of_two_pixelpitch;
     unsigned int         max_power_of_two_bytepitch;
     unsigned int         max_power_of_two_height;
};


static Chunk *split_chunk ( SurfaceManager *manager,
                            Chunk          *chunk,
                            int             length );

static Chunk *free_chunk  ( SurfaceManager *manager,
                            Chunk          *chunk );

static void   occupy_chunk( SurfaceManager *manager,
                            Chunk          *chunk,
                            SurfaceBuffer  *buffer,
                            int             length );


SurfaceManager *
dfb_surfacemanager_create( CoreDFB         *core,
                           CardLimitations *limits )
{
     SurfaceManager      *manager;
     FusionSHMPoolShared *pool;

     pool = dfb_core_shmpool( core );

     manager = SHCALLOC( pool, 1, sizeof(SurfaceManager) );
     if (!manager)
          return NULL;

     manager->shmpool          = pool;
     manager->byteoffset_align = limits->surface_byteoffset_alignment;
     manager->pixelpitch_align = limits->surface_pixelpitch_alignment;
     manager->bytepitch_align  = limits->surface_bytepitch_alignment;
     manager->max_power_of_two_pixelpitch = limits->surface_max_power_of_two_pixelpitch;
     manager->max_power_of_two_bytepitch  = limits->surface_max_power_of_two_bytepitch;
     manager->max_power_of_two_height     = limits->surface_max_power_of_two_height;

     fusion_skirmish_init( &manager->lock, "Surface Manager", dfb_core_world(core) );

     D_MAGIC_SET( manager, SurfaceManager );

     return manager;
}

DFBResult
dfb_surfacemanager_add_heap( SurfaceManager     *manager,
                             CoreSurfaceStorage  storage,
                             unsigned int        offset,
                             unsigned int        length )
{
     Heap  *heap, *first;
     Chunk *chunk;
     
     D_ASSERT( manager != NULL );

     if (storage != CSS_VIDEO && storage != CSS_AUXILIARY)
          return DFB_INVARG;

     heap = SHCALLOC( manager->shmpool, 1, sizeof(Heap) );
     if (!heap)
          return D_OOSHM();

     chunk = SHCALLOC( manager->shmpool, 1, sizeof(Chunk) );
     if (!chunk) {
          SHFREE( manager->shmpool, heap );
          return D_OOSHM();
     }

     D_MAGIC_SET( heap, _Heap_ );

     heap->storage = storage;
     heap->chunks  = chunk;
     heap->offset  = offset;
     heap->length  = length;
     heap->avail   = length - offset;

     D_MAGIC_SET( chunk, _Chunk_ );
     
     chunk->offset = offset;
     chunk->length = length - offset;
     chunk->heap   = heap;
   
     first = manager->heaps;
     if (first) {
          Heap *last = first->prev;
          heap->prev = last;
          last->next = first->prev = heap;
     }
     else {
          heap->prev = heap;
          manager->heaps = heap;
     }

     D_DEBUG_AT( Core_SM, "Added heap for storage 0x%x (offset %d, length %d).\n",
                 storage, offset, length );
     
     return DFB_OK;
}

void
dfb_surfacemanager_destroy( SurfaceManager *manager )
{
     Heap  *heap;
     Chunk *chunk;
     void  *next;

     D_ASSERT( manager != NULL );

     D_MAGIC_ASSERT( manager, SurfaceManager );

     /* Iterate through heaps. */
     heap = manager->heaps;
     while (heap) {
          /* Deallocate all video chunks. */
          chunk = heap->chunks;
          while (chunk) {
               next = chunk->next;

               D_MAGIC_CLEAR( chunk );

               SHFREE( manager->shmpool, chunk );

               chunk = next;
          }

          next = heap->next;

          D_MAGIC_CLEAR( heap );

          SHFREE( manager->shmpool, heap );

          heap = next;
     }

     D_MAGIC_CLEAR( manager );

     /* Destroy manager lock. */
     fusion_skirmish_destroy( &manager->lock );

     /* Deallocate manager struct. */
     SHFREE( manager->shmpool, manager );
}

DFBResult dfb_surfacemanager_suspend( SurfaceManager *manager )
{
     Heap  *h;
     Chunk *c;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

     h = manager->heaps;
     while (h) {
          c = h->chunks;
          while (c) {
               if (c->buffer) {
                    SurfaceBuffer *buffer = c->buffer;

                    switch (buffer->policy) {
                         case CSP_SYSTEMONLY:
                              break;

                         case CSP_VIDEOONLY:
                              /* FIXME */
                              break;

                         case CSP_VIDEOLOW:
                         case CSP_VIDEOHIGH:
                              switch (buffer->video.health) {
                                   case CSH_STORED:
                                        dfb_surfacemanager_assure_system( manager, buffer );
                                   case CSH_RESTORE:
                                        dfb_surfacemanager_deallocate( manager, buffer );
                                   default:
                                        break;
                              }
                              break;
                    }
               }

               c = c->next;
          }

          h = h->next;
     }

     manager->suspended = true;

     dfb_surfacemanager_unlock( manager );

     return DFB_OK;
}

DFBResult dfb_surfacemanager_resume( SurfaceManager *manager )
{
     Heap  *h;
     Chunk *c;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

     h = manager->heaps;
     while (h) {
          c = h->chunks;
          while (c) {
               if (c->buffer) {
                    SurfaceBuffer *buffer = c->buffer;

                    switch (buffer->policy) {
                         case CSP_SYSTEMONLY:
                              break;

                         case CSP_VIDEOONLY:
                              /* FIXME */
                              break;

                         case CSP_VIDEOLOW:
                         case CSP_VIDEOHIGH:
                              break;
                    }
               }

               c = c->next;
          }

          h = h->next;
     }

     manager->suspended = false;

     dfb_surfacemanager_unlock( manager );

     return DFB_OK;
}

void dfb_surfacemanager_lock( SurfaceManager *manager )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );

     fusion_skirmish_prevail( &manager->lock );
}

void dfb_surfacemanager_unlock( SurfaceManager *manager )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );

     fusion_skirmish_dismiss( &manager->lock );
}

DFBResult dfb_surfacemanager_adjust_heap_offset( SurfaceManager *manager,
                                                 int             offset )
{
     Heap *heap;
     
     D_ASSERT( offset >= 0 );
     D_ASSERT( manager->heaps != NULL );

     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

     if (manager->byteoffset_align > 1) {
          offset += manager->byteoffset_align - 1;
          offset -= offset % manager->byteoffset_align;
     }

     /*
      * Adjust only the offset of the first heap.
      */
     heap = manager->heaps;
     if (heap->chunks->buffer == NULL) {
          /* first chunk is free */
          if (offset <= heap->chunks->offset + heap->chunks->length) {
               /* ok, just recalculate offset and length */
               heap->chunks->length = heap->chunks->offset +
                                      heap->chunks->length - offset;
               heap->chunks->offset = offset;
          }
          else {
               D_WARN("unable to adjust heap offset");
               /* more space needed than free at the beginning */
               /* TODO: move/destroy instances */
          }
     }
     else {
          D_WARN("unable to adjust heap offset");
          /* very rare case that the first chunk is occupied */
          /* TODO: move/destroy instances */
     }

     heap->avail -= offset - heap->offset;
     heap->offset = offset;
     
     dfb_surfacemanager_unlock( manager );

     return DFB_OK;
}

void
dfb_surfacemanager_enumerate_chunks( SurfaceManager  *manager,
                                     SMChunkCallback  callback,
                                     void            *ctx )
{
     Heap  *h;
     Chunk *c;
     DFBEnumerationResult ret = DFENUM_OK;

     D_ASSERT( manager != NULL );
     D_ASSERT( callback != NULL );

     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

     h = manager->heaps;
     while (h && ret != DFENUM_CANCEL) {
          c = h->chunks;
          while (c && ret != DFENUM_CANCEL) {
               ret = callback( c->buffer, c->offset,
                               c->length, c->tolerations, ctx );

               c = c->next;
          }

          h = h->next;
     }

     dfb_surfacemanager_unlock( manager );
}

/** public functions NOT locking the surfacemanger theirself,
    to be called between lock/unlock of surfacemanager **/

DFBResult dfb_surfacemanager_allocate( SurfaceManager *manager,
                                       SurfaceBuffer  *buffer )
{
     int pitch;
     int length;
     Heap  *h;
     Chunk *c;

     Chunk *best_free = NULL;
     Chunk *best_occupied = NULL;
     
     CoreSurface *surface = buffer->surface;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     if (manager->suspended)
          return DFB_NOVIDEOMEMORY;

     /* calculate the required length depending on limitations */
     pitch = MAX( surface->width, surface->min_width );

     if (pitch < manager->max_power_of_two_pixelpitch &&
         surface->height < manager->max_power_of_two_height)
          pitch = 1 << direct_log2( pitch );

     if (manager->pixelpitch_align > 1) {
          pitch += manager->pixelpitch_align - 1;
          pitch -= pitch % manager->pixelpitch_align;
     }

     pitch = DFB_BYTES_PER_LINE( buffer->format, pitch );

     if (pitch < manager->max_power_of_two_bytepitch &&
         surface->height < manager->max_power_of_two_height)
          pitch = 1 << direct_log2( pitch );

     if (manager->bytepitch_align > 1) {
          pitch += manager->bytepitch_align - 1;
          pitch -= pitch % manager->bytepitch_align;
     }

     buffer->video.pitch = pitch;

     length = DFB_PLANE_MULTIPLY( buffer->format,
                                  MAX( surface->height, surface->min_height ) * pitch );

     /* Add extra space for optimized routines which are now allowed to overrun, e.g. prefetching. */
     length += 16;

     if (manager->byteoffset_align > 1) {
          length += manager->byteoffset_align - 1;
          length -= length % manager->byteoffset_align;
     }

     /* iterate through heaps */
     for (h = manager->heaps; h; h = h->next) { 
          if (length > h->avail)
               continue;

          /* examine chunks */
          c = h->chunks;
          while (c) {
               if (c->length >= length) {
                    if (c->buffer) {
                         c->tolerations++;
                         if (c->tolerations > 0xff)
                              c->tolerations = 0xff;

                         if (!c->buffer->video.locked              &&
                             c->buffer->policy <= buffer->policy   &&
                             c->buffer->policy != CSP_VIDEOONLY    &&
                            ((buffer->policy > c->buffer->policy)  ||
                             (c->tolerations > h->min_toleration/8 + 2)))
                         {
                              /* found a nice place to chill */
                              if (!best_occupied  ||
                                  best_occupied->length > c->length  ||
                                  best_occupied->tolerations < c->tolerations)
                                   /* first found or better one? */
                                   best_occupied = c;
                         }
                    }
                    else {
                         /* found a nice place to chill */
                         if (!best_free  ||  best_free->length > c->length)
                              /* first found or better one? */
                              best_free = c;
                    }
               }

               c = c->next;
          }

          /* if we found a place */
          if (best_free) {
               occupy_chunk( manager, best_free, buffer, length );

               return DFB_OK;
          }
     }

     if (best_occupied) {
          SurfaceBuffer *kicked = best_occupied->buffer;

          D_DEBUG_AT( Core_SM,
                      "Kicking out buffer at %d (%d) with tolerations %d...\n",
                      best_occupied->offset,
                      best_occupied->length, best_occupied->tolerations );

          dfb_surfacemanager_assure_system( manager, kicked );

          kicked->video.health = CSH_INVALID;
          dfb_surface_notify_listeners( kicked->surface, CSNF_VIDEO );

          best_occupied = free_chunk( manager, best_occupied );

          if (kicked->video.access & VAF_HARDWARE_READ) {
               dfb_gfxcard_sync();
               kicked->video.access &= ~VAF_HARDWARE_READ;
          }

          occupy_chunk( manager, best_occupied, buffer, length );

          return DFB_OK;
     }

     D_DEBUG_AT( Core_SM, "Couldn't allocate enough heap space for video memory surface!\n" );

     /* no luck */
     return DFB_NOVIDEOMEMORY;
}

DFBResult dfb_surfacemanager_deallocate( SurfaceManager *manager,
                                         SurfaceBuffer  *buffer )
{
     int    loops = 0;
     Chunk *chunk = buffer->video.chunk;

     D_ASSERT( buffer->surface );

     D_MAGIC_ASSERT( manager, SurfaceManager );

     if (buffer->video.health == CSH_INVALID)
          return DFB_OK;

     buffer->video.health = CSH_INVALID;
     buffer->video.chunk = NULL;

     dfb_surface_notify_listeners( buffer->surface, CSNF_VIDEO );

     while (buffer->video.locked) {
          if (++loops > 1000)
               break;

          sched_yield();
     }

     if (buffer->video.locked)
          D_WARN( "Freeing chunk with a non-zero lock counter" );

     /* If hardware has (to) read/write... */
     if (buffer->video.access & (VAF_HARDWARE_READ | VAF_HARDWARE_WRITE)) {
          /* ...wait for it. */
          dfb_gfxcard_sync(); /* TODO: wait for serial instead */

          /* ...clear hardware access. */
          buffer->video.access &= ~(VAF_HARDWARE_READ | VAF_HARDWARE_WRITE);
     }

     if (chunk)
          free_chunk( manager, chunk );

     return DFB_OK;
}

static void
transfer_buffer( SurfaceBuffer *buffer,
                 void          *src,
                 void          *dst,
                 int            srcpitch,
                 int            dstpitch )
{
     int          i;
     CoreSurface *surface = buffer->surface;

     for (i=0; i<surface->height; i++) {
          direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format, surface->width) );

          src += srcpitch;
          dst += dstpitch;
     }

     switch (buffer->format) {
          case DSPF_YV12:
          case DSPF_I420:
               for (i=0; i<surface->height; i++) {
                    direct_memcpy( dst, src,
                                   DFB_BYTES_PER_LINE( buffer->format, surface->width / 2 ) );
                    src += srcpitch / 2;
                    dst += dstpitch / 2;
               }
               break;

          case DSPF_NV12:
          case DSPF_NV21:
               for (i=0; i<surface->height/2; i++) {
                    direct_memcpy( dst, src,
                                   DFB_BYTES_PER_LINE( buffer->format, surface->width ) );
                    src += srcpitch;
                    dst += dstpitch;
               }
               break;

          case DSPF_NV16:
               for (i=0; i<surface->height; i++) {
                    direct_memcpy( dst, src,
                                   DFB_BYTES_PER_LINE( buffer->format, surface->width ) );
                    src += srcpitch;
                    dst += dstpitch;
               }
               break;

          default:
               break;
     }
}

DFBResult dfb_surfacemanager_assure_video( SurfaceManager *manager,
                                           SurfaceBuffer  *buffer )
{
     DFBResult    ret;
     CoreSurface *surface = buffer->surface;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     if (manager->suspended || buffer->flags & SBF_SUSPENDED)
          return DFB_SUSPENDED;

     switch (buffer->video.health) {
          case CSH_STORED:
               if (buffer->video.chunk)
                    buffer->video.chunk->tolerations = 0;
               else
                    buffer->storage = CSS_VIDEO;  /* for chunk less fbdev surface */

               break;

          case CSH_INVALID:
               /* Allocate video memory. */
               ret = dfb_surfacemanager_allocate( manager, buffer );
               if (ret)
                    return ret;

               /* fall through */

          case CSH_RESTORE: {
               Chunk *chunk = buffer->video.chunk;

               /* Upload? */
               if (buffer->flags & SBF_WRITTEN && buffer->system.health == CSH_STORED) {
                    void *video  = (chunk->heap->storage == CSS_VIDEO)
                                   ? dfb_system_video_memory_virtual( buffer->video.offset )
                                   : dfb_system_aux_memory_virtual( buffer->video.offset );
                    bool  locked = D_FLAGS_ARE_SET( buffer->video.access,
                                                    VAF_SOFTWARE_LOCK | VAF_SOFTWARE_WRITE );

                    if (!locked)
                         dfb_gfxcard_surface_enter( buffer, DSLF_WRITE );

                    /* Copy the data. */
                    transfer_buffer( buffer,
                                     buffer->system.addr, video,
                                     buffer->system.pitch, buffer->video.pitch );

                    if (!locked)
                         dfb_gfxcard_surface_leave( buffer );
               }

               /* Update health. */
               buffer->video.health = CSH_STORED;

               /* Reset tolerations. */
               chunk->tolerations = 0;

               dfb_surface_notify_listeners( surface, CSNF_VIDEO );

               /* Free system instance. */
               if (dfb_config->thrifty_surface_buffers) {
                    if (buffer->system.health && !(buffer->flags & SBF_FOREIGN_SYSTEM)) {
                         buffer->system.health = CSH_INVALID;

                         SHFREE( buffer->surface->shmpool_data, buffer->system.addr );
                         buffer->system.addr = NULL;
                    }
               }

               break;
          }

          default:
               D_BUG( "unknown buffer health %d", buffer->video.health );
               return DFB_BUG;
     }

     return DFB_OK;
}

DFBResult dfb_surfacemanager_assure_system( SurfaceManager *manager,
                                            SurfaceBuffer  *buffer )
{
     CoreSurface *surface = buffer->surface;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     if (buffer->policy == CSP_VIDEOONLY) {
          D_BUG( "surface_manager_assure_system() called on video only surface" );
          return DFB_BUG;
     }

     if (manager->suspended || buffer->flags & SBF_SUSPENDED)
          return DFB_SUSPENDED;

     switch (buffer->system.health) {
          case CSH_STORED:
               break;

          case CSH_INVALID:
               /* Allocate shared memory. */
               buffer->system.addr = SHMALLOC( surface->shmpool_data, buffer->system.size );
               if (!buffer->system.addr)
                    return D_OOSHM();

               /* fall through */

          case CSH_RESTORE: {
               Chunk *chunk = buffer->video.chunk;

               /* Download? */
               if (buffer->system.health == CSH_INVALID ||
                   (buffer->flags & SBF_WRITTEN && buffer->video.health == CSH_STORED))
               {
                    void *video  = (chunk->heap->storage == CSS_VIDEO)
                                   ? dfb_system_video_memory_virtual( buffer->video.offset )
                                   : dfb_system_aux_memory_virtual( buffer->video.offset );
                    bool  locked = D_FLAGS_ARE_SET( buffer->video.access,
                                                    VAF_SOFTWARE_LOCK | VAF_SOFTWARE_READ );

                    if (!locked)
                         dfb_gfxcard_surface_enter( buffer, DSLF_READ );


                    /* from video_access_by_software() in surface.c */
                    if (buffer->video.access & VAF_HARDWARE_WRITE) {
                         dfb_gfxcard_wait_serial( &buffer->video.serial );
                         dfb_gfxcard_flush_read_cache();
                         buffer->video.access &= ~VAF_HARDWARE_WRITE;
                    }
                    buffer->video.access |= VAF_SOFTWARE_READ;

                    /* Copy the data. */
                    transfer_buffer( buffer, video, buffer->system.addr,
                                     buffer->video.pitch, buffer->system.pitch );

                    if (!locked)
                         dfb_gfxcard_surface_leave( buffer );
               }

               /* Update health. */
               buffer->system.health = CSH_STORED;

               dfb_surface_notify_listeners( surface, CSNF_SYSTEM );
               break;
          }

          default:
               D_BUG( "unknown buffer health %d", buffer->system.health );
               return DFB_BUG;
     }

     return DFB_OK;
}

/** internal functions NOT locking the surfacemanager **/

static Chunk *
split_chunk( SurfaceManager *manager, Chunk *c, int length )
{
     Chunk *newchunk;

     D_MAGIC_ASSERT( c, _Chunk_ );
     D_MAGIC_ASSERT( c->heap, _Heap_ );

     if (c->length == length)          /* does not need be splitted */
          return c;

     newchunk = (Chunk*) SHCALLOC( manager->shmpool, 1, sizeof(Chunk) );

     /* calculate offsets and lengths of resulting chunks */
     newchunk->offset = c->offset + c->length - length;
     newchunk->length = length;
     newchunk->heap   = c->heap;
     c->length -= newchunk->length;

     /* insert newchunk after chunk c */
     newchunk->prev = c;
     newchunk->next = c->next;
     if (c->next)
          c->next->prev = newchunk;
     c->next = newchunk;

     D_MAGIC_SET( newchunk, _Chunk_ );

     return newchunk;
}

static Chunk *
free_chunk( SurfaceManager *manager, Chunk *chunk )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( chunk, _Chunk_ );
     D_MAGIC_ASSERT( chunk->heap, _Heap_ );

     if (!chunk->buffer) {
          D_BUG( "freeing free chunk" );
          return chunk;
     }
     else {
          D_DEBUG_AT( Core_SM, 
                      "Deallocating %d bytes at offset %d (storage 0x%x).\n",
                      chunk->length, chunk->offset, chunk->heap->storage );
     }

     if (chunk->buffer->policy == CSP_VIDEOONLY)
          chunk->heap->avail += chunk->length;

     chunk->buffer->storage = CSS_NONE;
     chunk->buffer = NULL;

     chunk->heap->min_toleration--;

     if (chunk->prev  &&  !chunk->prev->buffer) {
          Chunk *prev = chunk->prev;

          //D_DEBUG_AT( Core_SM, "  -> merging with previous chunk at %d\n", prev->offset );

          prev->length += chunk->length;

          prev->next = chunk->next;
          if (prev->next)
               prev->next->prev = prev;

          //D_DEBUG_AT( Core_SM, "  -> freeing %p (prev %p, next %p)\n", chunk, chunk->prev, chunk->next);

          D_MAGIC_CLEAR( chunk );

          SHFREE( manager->shmpool, chunk );
          chunk = prev;
     }

     if (chunk->next  &&  !chunk->next->buffer) {
          Chunk *next = chunk->next;

          //D_DEBUG_AT( Core_SM, "  -> merging with next chunk at %d\n", next->offset );

          chunk->length += next->length;

          chunk->next = next->next;
          if (chunk->next)
               chunk->next->prev = chunk;

          D_MAGIC_CLEAR( next );

          SHFREE( manager->shmpool, next );
     }

     return chunk;
}

static void
occupy_chunk( SurfaceManager *manager, Chunk *chunk, SurfaceBuffer *buffer, int length )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( chunk, _Chunk_ );
     D_MAGIC_ASSERT( chunk->heap, _Heap_ );
     
     if (buffer->policy == CSP_VIDEOONLY)
          chunk->heap->avail -= length;

     chunk = split_chunk( manager, chunk, length );

     D_DEBUG_AT( Core_SM, "Allocating %d bytes at offset %d (storage 0x%x).\n",
                 chunk->length, chunk->offset, chunk->heap->storage );

     buffer->storage      = chunk->heap->storage;
     buffer->video.health = CSH_RESTORE;
     buffer->video.offset = chunk->offset;
     buffer->video.chunk  = chunk;

     chunk->buffer = buffer;

     chunk->heap->min_toleration++;
}

