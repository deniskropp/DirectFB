/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>


D_DEBUG_DOMAIN( Core_SM, "Core/SurfaceMgr", "DirectFB Surface Manager" );

/*
 * initially there is one big free chunk,
 * chunks are splitted into a free and an occupied chunk if memory is allocated,
 * two chunks are merged to one free chunk if memory is deallocated
 */
struct _Chunk {
     int             magic;

     int             offset;      /* offset in video memory,
                                     is greater or equal to the heap offset */
     int             length;      /* length of this chunk in bytes */

     SurfaceBuffer  *buffer;      /* pointer to surface buffer occupying
                                     this chunk, or NULL if chunk is free */

     int             tolerations; /* number of times this chunk was scanned
                                     occupied, resetted in assure_video */

     Chunk          *prev;
     Chunk          *next;
};

struct _SurfaceManager {
     int             magic;

     FusionSkirmish  lock;

     Chunk          *chunks;
     int             length;
     int             available;

     int             min_toleration;

     bool            suspended;

     /* offset of the surface heap */
     unsigned int    heap_offset;

     /* card limitations for surface offsets and their pitch */
     unsigned int    byteoffset_align;
     unsigned int    pixelpitch_align;
     unsigned int    bytepitch_align;

     unsigned int    max_power_of_two_pixelpitch;
     unsigned int    max_power_of_two_bytepitch;
     unsigned int    max_power_of_two_height;
};


static Chunk* split_chunk( Chunk *c, int length );
static Chunk* free_chunk( SurfaceManager *manager, Chunk *chunk );
static void occupy_chunk( SurfaceManager *manager, Chunk *chunk, SurfaceBuffer *buffer, int length );


SurfaceManager *
dfb_surfacemanager_create( unsigned int     length,
                           CardLimitations *limits )
{
     Chunk          *chunk;
     SurfaceManager *manager;

     manager = SHCALLOC( 1, sizeof(SurfaceManager) );
     if (!manager)
          return NULL;

     chunk = SHCALLOC( 1, sizeof(Chunk) );
     if (!chunk) {
          SHFREE( manager );
          return NULL;
     }

     chunk->offset = 0;
     chunk->length = length;

     manager->chunks           = chunk;
     manager->length           = length;
     manager->available        = length;
     manager->byteoffset_align = limits->surface_byteoffset_alignment;
     manager->pixelpitch_align = limits->surface_pixelpitch_alignment;
     manager->bytepitch_align  = limits->surface_bytepitch_alignment;
     manager->max_power_of_two_pixelpitch = limits->surface_max_power_of_two_pixelpitch;
     manager->max_power_of_two_bytepitch  = limits->surface_max_power_of_two_bytepitch;
     manager->max_power_of_two_height     = limits->surface_max_power_of_two_height;

     fusion_skirmish_init( &manager->lock, "Surface Manager" );

     D_MAGIC_SET( chunk, _Chunk_ );

     D_MAGIC_SET( manager, SurfaceManager );

     return manager;
}

void
dfb_surfacemanager_destroy( SurfaceManager *manager )
{
     Chunk *chunk;

     D_ASSERT( manager != NULL );
     D_ASSERT( manager->chunks != NULL );

     D_MAGIC_ASSERT( manager, SurfaceManager );

     D_MAGIC_CLEAR( manager );

     /* Deallocate all chunks. */
     chunk = manager->chunks;
     while (chunk) {
          Chunk *next = chunk->next;

          D_MAGIC_CLEAR( chunk );

          SHFREE( chunk );

          chunk = next;
     }

     /* Destroy manager lock. */
     fusion_skirmish_destroy( &manager->lock );

     /* Deallocate manager struct. */
     SHFREE( manager );
}

DFBResult dfb_surfacemanager_suspend( SurfaceManager *manager )
{
     Chunk *c;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

     manager->suspended = true;

     c = manager->chunks;
     while (c) {
          if (c->buffer &&
              c->buffer->policy != CSP_VIDEOONLY &&
              c->buffer->video.health == CSH_STORED)
          {
               dfb_surfacemanager_assure_system( manager, c->buffer );
               c->buffer->video.health = CSH_RESTORE;
          }

          c = c->next;
     }

     dfb_surfacemanager_unlock( manager );

     return DFB_OK;
}

DFBResult dfb_surfacemanager_resume( SurfaceManager *manager )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

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
     D_ASSERT( offset >= 0 );

     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

     if (manager->byteoffset_align > 1) {
          offset += manager->byteoffset_align - 1;
          offset -= offset % manager->byteoffset_align;
     }

     if (manager->chunks->buffer == NULL) {
          /* first chunk is free */
          if (offset <= manager->chunks->offset + manager->chunks->length) {
               /* ok, just recalculate offset and length */
               manager->chunks->length = manager->chunks->offset + manager->chunks->length - offset;
               manager->chunks->offset = offset;
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

     manager->heap_offset = offset;

     dfb_surfacemanager_unlock( manager );

     return DFB_OK;
}

void
dfb_surfacemanager_enumerate_chunks( SurfaceManager  *manager,
                                     SMChunkCallback  callback,
                                     void            *ctx )
{
     Chunk *c;

     D_ASSERT( manager != NULL );
     D_ASSERT( callback != NULL );

     D_MAGIC_ASSERT( manager, SurfaceManager );

     dfb_surfacemanager_lock( manager );

     c = manager->chunks;
     while (c) {
          if (callback( c->buffer, c->offset,
                        c->length, c->tolerations, ctx) == DFENUM_CANCEL)
               break;

          c = c->next;
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
     Chunk *c;

     Chunk *best_free = NULL;
     Chunk *best_occupied = NULL;

     CoreSurface *surface = buffer->surface;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     if (!manager->length || manager->suspended)
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

     length = DFB_PLANE_MULTIPLY( buffer->format,
                                  MAX( surface->height, surface->min_height ) * pitch );

     if (manager->byteoffset_align > 1) {
          length += manager->byteoffset_align - 1;
          length -= length % manager->byteoffset_align;
     }

     /* Do a pre check before iterating through all chunks. */
     if (length > manager->available - manager->heap_offset)
          return DFB_NOVIDEOMEMORY;

     buffer->video.pitch = pitch;

     /* examine chunks */
     c = manager->chunks;
     while (c) {
          if (c->length >= length) {
               if (c->buffer) {
                    c->tolerations++;
                    if (c->tolerations > 0xff)
                         c->tolerations = 0xff;

                    if (!c->buffer->video.locked              &&
                        c->buffer->policy <= buffer->policy   &&
                        c->buffer->policy != CSP_VIDEOONLY    &&

                        ((buffer->policy > c->buffer->policy) ||
                         (c->tolerations > manager->min_toleration/8 + 2)))
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

     if (best_occupied) {
          SurfaceBuffer *kicked = best_occupied->buffer;

          D_DEBUG_AT( Core_SM, "Kicking out buffer at %d (%d) with tolerations %d...\n",
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

     if (chunk)
          free_chunk( manager, chunk );

     return DFB_OK;
}

DFBResult dfb_surfacemanager_assure_video( SurfaceManager *manager,
                                           SurfaceBuffer  *buffer )
{
     DFBResult    ret;
     CoreSurface *surface = buffer->surface;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     if (manager->suspended)
          return DFB_NOVIDEOMEMORY;

     switch (buffer->video.health) {
          case CSH_STORED:
               if (buffer->video.chunk)
                    buffer->video.chunk->tolerations = 0;

               return DFB_OK;

          case CSH_INVALID:
               ret = dfb_surfacemanager_allocate( manager, buffer );
               if (ret)
                    return ret;

               /* FALL THROUGH, because after successful allocation
                  the surface health is CSH_RESTORE */

          case CSH_RESTORE:
               if (buffer->system.health != CSH_STORED)
                    D_BUG( "system/video instances both not stored!" );

               if (buffer->flags & SBF_WRITTEN) {
                    int   i;
                    char *src = buffer->system.addr;
                    char *dst = dfb_system_video_memory_virtual( buffer->video.offset );

                    for (i=0; i<surface->height; i++) {
                         direct_memcpy( dst, src,
                                        DFB_BYTES_PER_LINE(buffer->format, surface->width) );
                         src += buffer->system.pitch;
                         dst += buffer->video.pitch;
                    }

                    if (buffer->format == DSPF_YV12 || buffer->format == DSPF_I420) {
                         for (i=0; i<surface->height; i++) {
                              direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format,
                                                                          surface->width / 2) );
                              src += buffer->system.pitch / 2;
                              dst += buffer->video.pitch  / 2;
                         }
                    }
                    else if (buffer->format == DSPF_NV12 || buffer->format == DSPF_NV21) {
                         for (i=0; i<surface->height/2; i++) {
                              direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format,
                                                                          surface->width) );
                              src += buffer->system.pitch;
                              dst += buffer->video.pitch;
                         }
                    }
                    else if (buffer->format == DSPF_NV16) {
                         for (i=0; i<surface->height; i++) {
                              direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format,
                                                                          surface->width) );
                              src += buffer->system.pitch;
                              dst += buffer->video.pitch;
                         }
                    }
               }

               buffer->video.health             = CSH_STORED;
               buffer->video.chunk->tolerations = 0;

               dfb_surface_notify_listeners( surface, CSNF_VIDEO );

               return DFB_OK;

          default:
               break;
     }

     D_BUG( "unknown video instance health" );
     return DFB_BUG;
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

     if (buffer->system.health == CSH_STORED)
          return DFB_OK;
     else if (buffer->video.health == CSH_STORED) {
          int   i;
          char *src = dfb_system_video_memory_virtual( buffer->video.offset );
          char *dst = buffer->system.addr;

          /* from video_access_by_software() in surface.c */
          if (buffer->video.access & VAF_HARDWARE_WRITE) {
               dfb_gfxcard_wait_serial( &buffer->video.serial );
               dfb_gfxcard_flush_read_cache();
               buffer->video.access &= ~VAF_HARDWARE_WRITE;
          }
          buffer->video.access |= VAF_SOFTWARE_READ;

          for (i=0; i<surface->height; i++) {
               direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format, surface->width) );
               src += buffer->video.pitch;
               dst += buffer->system.pitch;
          }

          if (buffer->format == DSPF_YV12 || buffer->format == DSPF_I420) {
               for (i=0; i<surface->height; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format,
                                                                surface->width / 2) );
                    src += buffer->video.pitch  / 2;
                    dst += buffer->system.pitch / 2;
               }
          }
          else if (buffer->format == DSPF_NV12 || buffer->format == DSPF_NV21) {
               for (i=0; i<surface->height/2; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format,
                                                                surface->width) );
                    src += buffer->video.pitch;
                    dst += buffer->system.pitch;
               }
          }
          else if (buffer->format == DSPF_NV16) {
               for (i=0; i<surface->height; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE(buffer->format,
                                                                surface->width) );
                    src += buffer->video.pitch;
                    dst += buffer->system.pitch;
               }
          }

          buffer->system.health = CSH_STORED;

          dfb_surface_notify_listeners( surface, CSNF_SYSTEM );

          return DFB_OK;
     }

     D_BUG( "no valid surface instance" );
     return DFB_BUG;
}

/** internal functions NOT locking the surfacemanager **/

static Chunk* split_chunk( Chunk *c, int length )
{
     Chunk *newchunk;

     D_MAGIC_ASSERT( c, _Chunk_ );

     if (c->length == length)          /* does not need be splitted */
          return c;

     newchunk = (Chunk*) SHCALLOC( 1, sizeof(Chunk) );

     /* calculate offsets and lengths of resulting chunks */
     newchunk->offset = c->offset + c->length - length;
     newchunk->length = length;
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

static Chunk*
free_chunk( SurfaceManager *manager, Chunk *chunk )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( chunk, _Chunk_ );

     if (!chunk->buffer) {
          D_BUG( "freeing free chunk" );
          return chunk;
     }
     else {
          D_DEBUG_AT( Core_SM, "Deallocating %d bytes at offset %d.\n",
                      chunk->length, chunk->offset );
     }

     if (chunk->buffer->policy == CSP_VIDEOONLY)
          manager->available += chunk->length;

     chunk->buffer = NULL;

     manager->min_toleration--;

     if (chunk->prev  &&  !chunk->prev->buffer) {
          Chunk *prev = chunk->prev;

          //D_DEBUG_AT( Core_SM, "  -> merging with previous chunk at %d\n", prev->offset );

          prev->length += chunk->length;

          prev->next = chunk->next;
          if (prev->next)
               prev->next->prev = prev;

          //D_DEBUG_AT( Core_SM, "  -> freeing %p (prev %p, next %p)\n", chunk, chunk->prev, chunk->next);

          D_MAGIC_CLEAR( chunk );

          SHFREE( chunk );
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

          SHFREE( next );
     }

     return chunk;
}

static void
occupy_chunk( SurfaceManager *manager, Chunk *chunk, SurfaceBuffer *buffer, int length )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( chunk, _Chunk_ );

     if (buffer->policy == CSP_VIDEOONLY)
          manager->available -= length;

     chunk = split_chunk( chunk, length );

     D_DEBUG_AT( Core_SM, "Allocating %d bytes at offset %d.\n", chunk->length, chunk->offset );

     buffer->video.health = CSH_RESTORE;
     buffer->video.offset = chunk->offset;
     buffer->video.chunk  = chunk;

     chunk->buffer = buffer;

     manager->min_toleration++;
}

