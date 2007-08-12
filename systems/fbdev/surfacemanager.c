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

#include <fusion/shmalloc.h>

#include <directfb.h>

#include <core/core.h>

#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <gfx/convert.h>

#include "surfacemanager.h"

D_DEBUG_DOMAIN( SurfMan, "SurfaceManager", "DirectFB Surface Manager" );

struct _SurfaceManager {
     int                  magic;

     FusionSHMPoolShared *shmpool;

     Chunk               *chunks;

     int                  offset;
     int                  length;         /* length of the heap in bytes */
     int                  avail;          /* amount of available memory in bytes */

     int                  min_toleration;
     
     bool                 suspended;
};


static Chunk *split_chunk ( SurfaceManager *manager,
                            Chunk          *chunk,
                            int             length );

static Chunk *free_chunk  ( SurfaceManager *manager,
                            Chunk          *chunk );

static Chunk *occupy_chunk( SurfaceManager     *manager,
                            Chunk              *chunk,
                            CoreSurfaceBuffer  *buffer,
                            int                 length,
                            int                 pitch );


DFBResult
dfb_surfacemanager_create( CoreDFB         *core,
                           unsigned int     length,
                           SurfaceManager **ret_manager )
{
     FusionSHMPoolShared *pool;
     SurfaceManager      *manager;
     Chunk               *chunk;

     D_ASSERT( core != NULL );
     D_ASSERT( ret_manager != NULL );

     pool = dfb_core_shmpool( core );

     manager = SHCALLOC( pool, 1, sizeof(SurfaceManager) );
     if (!manager)
          return D_OOSHM();

     chunk = SHCALLOC( pool, 1, sizeof(Chunk) );
     if (!chunk) {
          D_OOSHM();
          SHFREE( pool, manager );
          return DFB_NOSHAREDMEMORY;
     }

     manager->shmpool = pool;
     manager->chunks  = chunk;
     manager->offset  = 0;
     manager->length  = length;
     manager->avail   = manager->length - manager->offset;

     D_MAGIC_SET( manager, SurfaceManager );

     chunk->offset    = manager->offset;
     chunk->length    = manager->avail;

     D_MAGIC_SET( chunk, Chunk );

     *ret_manager = manager;

     return DFB_OK;
}

void
dfb_surfacemanager_destroy( SurfaceManager *manager )
{
     Chunk *chunk;
     void  *next;

     D_MAGIC_ASSERT( manager, SurfaceManager );

     /* Deallocate all video chunks. */
     chunk = manager->chunks;
     while (chunk) {
          next = chunk->next;

          D_MAGIC_CLEAR( chunk );

          SHFREE( manager->shmpool, chunk );

          chunk = next;
     }

     D_MAGIC_CLEAR( manager );

     /* Deallocate manager struct. */
     SHFREE( manager->shmpool, manager );
}

DFBResult dfb_surfacemanager_adjust_heap_offset( SurfaceManager *manager,
                                                 int             offset )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_ASSERT( offset >= 0 );

/*FIXME_SC_2     if (manager->limits.surface_byteoffset_alignment > 1) {
          offset += manager->limits.surface_byteoffset_alignment - 1;
          offset -= offset % manager->limits.surface_byteoffset_alignment;
     }
*/
     /*
      * Adjust the offset of the heap.
      */
     if (manager->chunks->buffer == NULL) {
          /* first chunk is free */
          if (offset <= manager->chunks->offset + manager->chunks->length) {
               /* ok, just recalculate offset and length */
               manager->chunks->length = manager->chunks->offset +
                                         manager->chunks->length - offset;
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

     manager->avail -= offset - manager->offset;
     manager->offset = offset;

     return DFB_OK;
}

/** public functions NOT locking the surfacemanger theirself,
    to be called between lock/unlock of surfacemanager **/

DFBResult dfb_surfacemanager_allocate( CoreDFB            *core,
                                       SurfaceManager     *manager,
                                       CoreSurfaceBuffer  *buffer,
                                       Chunk             **ret_chunk )
{
     int pitch;
     int length;
     Chunk *c;
     CoreGraphicsDevice *device;

     Chunk *best_free     = NULL;
     Chunk *best_occupied = NULL;

     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
//NULL means check only     D_ASSERT( ret_chunk != NULL );

     D_DEBUG_AT( SurfMan, "%s( %p ) <- %dx%d %s\n", __FUNCTION__, buffer,
                 buffer->surface->config.size.w, buffer->surface->config.size.h,
                 dfb_pixelformat_name( buffer->surface->config.format ) );

     if (manager->suspended)
          return DFB_SUSPENDED;

     /* FIXME: Only one global device at the moment. */
     device = dfb_core_get_part( core, DFCP_GRAPHICS );
     D_ASSERT( device != NULL );

     dfb_gfxcard_calc_buffer_size( device, buffer, &pitch, &length );

     D_DEBUG_AT( SurfMan, "  -> pitch %d, length %d\n", pitch, length );

     /* examine chunks */
     c = manager->chunks;
     D_MAGIC_ASSERT( c, Chunk );

     /* FIXME_SC_2  Workaround creation happening before graphics driver initialization. */
     if (!c->next) {
          int length = dfb_gfxcard_memory_length();

          if (c->length != length - manager->offset) {
               D_WARN( "workaround" );

               manager->length = length;
               manager->avail  = length - manager->offset;

               c->length = length - manager->offset;
          }
     }

     while (c) {
          D_MAGIC_ASSERT( c, Chunk );

          if (c->length >= length) {
               if (c->buffer) {
                    c->tolerations++;
                    if (c->tolerations > 0xff)
                         c->tolerations = 0xff;

                    if (//FIXME_SC_1  !c->buffer->video.locked              &&
                        c->buffer->policy <= buffer->policy   &&
                        c->buffer->policy != CSP_VIDEOONLY    &&
                       ((buffer->policy > c->buffer->policy)  ||
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
          /* NULL means check only. */
          if (ret_chunk)
               *ret_chunk = occupy_chunk( manager, best_free, buffer, length, pitch );

          return DFB_OK;
     }
/*
     if (best_occupied) {
          CoreSurfaceBuffer *kicked = best_occupied->buffer;

          D_DEBUG_AT( SurfMan,
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
*/
     D_DEBUG_AT( SurfMan, "Couldn't allocate enough heap space for video memory surface!\n" );

     /* no luck */
     return DFB_NOVIDEOMEMORY;
}

DFBResult dfb_surfacemanager_deallocate( SurfaceManager *manager,
                                         Chunk          *chunk )
{
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( chunk, Chunk );

     buffer = chunk->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     D_DEBUG_AT( SurfMan, "%s( %p ) <- %dx%d %s\n", __FUNCTION__, buffer,
                 buffer->surface->config.size.w, buffer->surface->config.size.h,
                 dfb_pixelformat_name( buffer->surface->config.format ) );

     free_chunk( manager, chunk );

     return DFB_OK;
}

/*
static void
set_sentinel( CoreSurface *surface, CoreSurfaceBuffer *buffer )
{
     int   i;
     void *start    = dfb_system_video_memory_virtual( buffer->video.offset );
     char *sentinel = start + buffer->video.pitch * DFB_PLANE_MULTIPLY( buffer->format,
                                                                        surface->config.size.h );
 
     for (i=0; i<16; i++)
          sentinel[i] = i;
}
*/

/** internal functions NOT locking the surfacemanager **/

static Chunk *
split_chunk( SurfaceManager *manager, Chunk *c, int length )
{
     Chunk *newchunk;

     D_MAGIC_ASSERT( c, Chunk );

     if (c->length == length)          /* does not need be splitted */
          return c;

     newchunk = (Chunk*) SHCALLOC( manager->shmpool, 1, sizeof(Chunk) );

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

     D_MAGIC_SET( newchunk, Chunk );

     return newchunk;
}

static Chunk *
free_chunk( SurfaceManager *manager, Chunk *chunk )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( chunk, Chunk );

     if (!chunk->buffer) {
          D_BUG( "freeing free chunk" );
          return chunk;
     }
     else {
          D_DEBUG_AT( SurfMan, "Deallocating %d bytes at offset %d.\n", chunk->length, chunk->offset );
     }

     if (chunk->buffer->policy == CSP_VIDEOONLY)
          manager->avail += chunk->length;

     chunk->buffer = NULL;

     manager->min_toleration--;

     if (chunk->prev  &&  !chunk->prev->buffer) {
          Chunk *prev = chunk->prev;

          //D_DEBUG_AT( SurfMan, "  -> merging with previous chunk at %d\n", prev->offset );

          prev->length += chunk->length;

          prev->next = chunk->next;
          if (prev->next)
               prev->next->prev = prev;

          //D_DEBUG_AT( SurfMan, "  -> freeing %p (prev %p, next %p)\n", chunk, chunk->prev, chunk->next);

          D_MAGIC_CLEAR( chunk );

          SHFREE( manager->shmpool, chunk );
          chunk = prev;
     }

     if (chunk->next  &&  !chunk->next->buffer) {
          Chunk *next = chunk->next;

          //D_DEBUG_AT( SurfMan, "  -> merging with next chunk at %d\n", next->offset );

          chunk->length += next->length;

          chunk->next = next->next;
          if (chunk->next)
               chunk->next->prev = chunk;

          D_MAGIC_CLEAR( next );

          SHFREE( manager->shmpool, next );
     }

     return chunk;
}

static Chunk *
occupy_chunk( SurfaceManager *manager, Chunk *chunk, CoreSurfaceBuffer *buffer, int length, int pitch )
{
     D_MAGIC_ASSERT( manager, SurfaceManager );
     D_MAGIC_ASSERT( chunk, Chunk );
     
     if (buffer->policy == CSP_VIDEOONLY)
          manager->avail -= length;

     chunk = split_chunk( manager, chunk, length );

     D_DEBUG_AT( SurfMan, "Allocating %d bytes at offset %d.\n", chunk->length, chunk->offset );

     chunk->buffer = buffer;
     chunk->pitch  = pitch;

     manager->min_toleration++;

//     set_sentinel( buffer->surface, buffer );

     return chunk;
}

