/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include <pthread.h>

#include <core/fusion/list.h>
#include <core/fusion/shmalloc.h>

#include "directfb.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"

#include "gfxcard.h"
#include "surfaces.h"
#include "surfacemanager.h"
#include "system.h"

#include "misc/util.h"
#include "misc/mem.h"
#include "misc/memcpy.h"

/*
 * initially there is one big free chunk,
 * chunks are splitted into a free and an occupied chunk if memory is allocated,
 * two chunks are merged to one free chunk if memory is deallocated
 */
struct _Chunk {
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
     FusionSkirmish  lock;

     Chunk          *chunks;
     int             length;
     int             available;

     bool            suspended;

     /* offset of the surface heap */
     unsigned int    heap_offset;

     /* card limitations for surface offsets and their pitch */
     unsigned int    byteoffset_align;
     unsigned int    pixelpitch_align;
};


static int min_toleration = 8;

static Chunk* split_chunk( Chunk *c, int length );
static Chunk* free_chunk( SurfaceManager *manager, Chunk *chunk );
static void occupy_chunk( SurfaceManager *manager, Chunk *chunk, SurfaceBuffer *buffer, int length );


SurfaceManager *
dfb_surfacemanager_create( unsigned int length,
                           unsigned int byteoffset_align,
                           unsigned int pixelpitch_align )
{
     Chunk          *chunk;
     SurfaceManager *manager;

     manager = shcalloc( 1, sizeof(SurfaceManager) );
     if (!manager)
          return NULL;

     chunk = shcalloc( 1, sizeof(Chunk) );
     if (!chunk) {
          shfree( manager );
          return NULL;
     }

     chunk->offset = 0;
     chunk->length = length;

     manager->chunks           = chunk;
     manager->length           = length;
     manager->available        = length;
     manager->byteoffset_align = byteoffset_align;
     manager->pixelpitch_align = pixelpitch_align;

     fusion_skirmish_init( &manager->lock );

     return manager;
}

void
dfb_surfacemanager_destroy( SurfaceManager *manager )
{
     Chunk *chunk;

     DFB_ASSERT( manager != NULL );
     DFB_ASSERT( manager->chunks != NULL );

     /* Deallocate all chunks. */
     chunk = manager->chunks;
     while (chunk) {
          Chunk *next = chunk->next;

          shfree( chunk );

          chunk = next;
     }

     /* Destroy manager lock. */
     fusion_skirmish_destroy( &manager->lock );
     
     /* Deallocate manager struct. */
     shfree( manager );
}

DFBResult dfb_surfacemanager_suspend( SurfaceManager *manager )
{
     Chunk *c;

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
     dfb_surfacemanager_lock( manager );
     
     manager->suspended = false;
     
     dfb_surfacemanager_unlock( manager );
     
     return DFB_OK;
}

void dfb_surfacemanager_lock( SurfaceManager *manager )
{
     fusion_skirmish_prevail( &manager->lock );
}

void dfb_surfacemanager_unlock( SurfaceManager *manager )
{
     fusion_skirmish_dismiss( &manager->lock );
}

DFBResult dfb_surfacemanager_adjust_heap_offset( SurfaceManager *manager,
                                                 int             offset )
{
     DFB_ASSERT( offset >= 0 );

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
               CAUTION("unable to adjust heap offset");
               /* more space needed than free at the beginning */
               /* TODO: move/destroy instances */
          }
     }
     else {
          CAUTION("unable to adjust heap offset");
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

     DFB_ASSERT( manager != NULL );
     DFB_ASSERT( callback != NULL );

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

     if (!manager->length || manager->suspended)
          return DFB_NOVIDEOMEMORY;

     /* calculate the required length depending on limitations */
     pitch = MAX( surface->width, surface->min_width );
     if (manager->pixelpitch_align > 1) {
          pitch += manager->pixelpitch_align - 1;
          pitch -= pitch % manager->pixelpitch_align;
     }

     pitch = DFB_BYTES_PER_LINE( surface->format, pitch );
     length = DFB_PLANE_MULTIPLY( surface->format,
                                  MAX( surface->height,
                                       surface->min_height ) * pitch );

     if (manager->byteoffset_align > 1) {
          length += manager->byteoffset_align - 1;
          length -= length % manager->byteoffset_align;
     }

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
                        c->buffer->policy != CSP_VIDEOONLY    &&
                        c->buffer->policy <= buffer->policy   &&
                        ((c->tolerations > min_toleration/8) ||
                         buffer->policy == CSP_VIDEOONLY))
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
          /*
             debug_linear_fill( manager, best_free->offset,
                                best_free->length, 0x90, 0x90, 0x90 );
             debug_pause( manager );
          */

          occupy_chunk( manager, best_free, buffer, length );
     } else
     if (best_occupied) {
          CoreSurface *kicked = best_occupied->buffer->surface;

          DEBUGMSG( "kicking out surface at %d with tolerations %d...\n",
                    best_occupied->offset, best_occupied->tolerations );

          dfb_surfacemanager_assure_system( manager, best_occupied->buffer );

          best_occupied->buffer->video.health = CSH_INVALID;
          dfb_surface_notify_listeners( kicked, CSNF_VIDEO );

          best_occupied = free_chunk( manager, best_occupied );

          dfb_gfxcard_sync();

          DEBUGMSG( "kicked out.\n" );


          occupy_chunk( manager, best_occupied, buffer, length );
     }
     else {
          DEBUGMSG( "DirectFB/core/surfacemanager: "
                    "Couldn't allocate enough heap space "
                    "for video memory surface!\n" );

          /* no luck */
          return DFB_NOVIDEOMEMORY;
     }

     return DFB_OK;
}

DFBResult dfb_surfacemanager_deallocate( SurfaceManager *manager,
                                         SurfaceBuffer  *buffer )
{
     int    loops = 0;
     Chunk *chunk = buffer->video.chunk;

     DFB_ASSERT( buffer->surface );

     if (buffer->video.health == CSH_INVALID)
          return DFB_OK;

     DEBUGMSG( "deallocating...\n" );

     buffer->video.health = CSH_INVALID;
     buffer->video.chunk = NULL;

     dfb_surface_notify_listeners( buffer->surface, CSNF_VIDEO );

     while (buffer->video.locked) {
          if (++loops > 1000)
               break;
          
          sched_yield();
     }

     if (buffer->video.locked)
          CAUTION( "Freeing chunk with a non-zero lock counter" );

     if (chunk)
          free_chunk( manager, chunk );
     
     DEBUGMSG( "deallocated.\n" );

     return DFB_OK;
}

DFBResult dfb_surfacemanager_assure_video( SurfaceManager *manager,
                                           SurfaceBuffer  *buffer )
{
     CoreSurface *surface = buffer->surface;

     switch (buffer->video.health) {
          case CSH_STORED:
               if (buffer->video.chunk &&
                   buffer->video.chunk->tolerations != 0)
               {
                    buffer->video.chunk->tolerations = 0;
               }
               return DFB_OK;

          case CSH_INVALID: {
               DFBResult ret;

               ret = dfb_surfacemanager_allocate( manager, buffer );
               if (ret)
                    return ret;

               /* FALL THROUGH, because after successful allocation
                  the surface health is CSH_RESTORE */
          }
          case CSH_RESTORE: {
               int   h   = surface->height;
               char *src = buffer->system.addr;
               char *dst = dfb_system_video_memory_virtual( buffer->video.offset );

               if (buffer->system.health != CSH_STORED)
                    BUG( "system/video instances both not stored!" );

               while (h--) {
                    dfb_memcpy( dst, src, DFB_BYTES_PER_LINE(surface->format,
                                                             surface->width) );
                    src += buffer->system.pitch;
                    dst += buffer->video.pitch;
               }
               if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
                    h = surface->height;
                    while (h--) {
                         dfb_memcpy( dst, src, DFB_BYTES_PER_LINE(surface->format,
                                                                  surface->width / 2) );
                         src += buffer->system.pitch / 2;
                         dst += buffer->video.pitch  / 2;
                    }
               }

               buffer->video.health = CSH_STORED;
               buffer->video.chunk->tolerations = 0;
               dfb_surface_notify_listeners( surface, CSNF_VIDEO );

               return DFB_OK;
          }
     }

     BUG( "unknown video instance health" );
     return DFB_BUG;
}

DFBResult dfb_surfacemanager_assure_system( SurfaceManager *manager,
                                            SurfaceBuffer  *buffer )
{
     CoreSurface *surface = buffer->surface;

     if (buffer->policy == CSP_VIDEOONLY) {
          BUG( "surface_manager_assure_system() called on video only surface" );
          return DFB_BUG;
     }

     if (buffer->system.health == CSH_STORED)
          return DFB_OK;
     else if (buffer->video.health == CSH_STORED) {
          int   h   = surface->height;
          char *src = dfb_system_video_memory_virtual( buffer->video.offset );
          char *dst = buffer->system.addr;

          /* from video_access_by_software() in surface.c */
          if (buffer->video.access & VAF_HARDWARE_WRITE) {
               dfb_gfxcard_sync();
               buffer->video.access &= ~VAF_HARDWARE_WRITE;
          }
          buffer->video.access |= VAF_SOFTWARE_READ;

          while (h--) {
               dfb_memcpy( dst, src, DFB_BYTES_PER_LINE(surface->format,
                                                        surface->width) );
               src += buffer->video.pitch;
               dst += buffer->system.pitch;
          }
          if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
               h = surface->height;
               while (h--) {
                    dfb_memcpy( dst, src, DFB_BYTES_PER_LINE(surface->format,
                                                             surface->width / 2) );
                    src += buffer->video.pitch  / 2;
                    dst += buffer->system.pitch / 2;
               }
          }
          buffer->system.health = CSH_STORED;

          dfb_surface_notify_listeners( surface, CSNF_SYSTEM );

          return DFB_OK;
     }

     BUG( "no valid surface instance" );
     return DFB_BUG;
}

/** internal functions NOT locking the surfacemanager **/

static Chunk* split_chunk( Chunk *c, int length )
{
     Chunk *newchunk;

     if (c->length == length)          /* does not need be splitted */
          return c;

     newchunk = (Chunk*) shcalloc( 1, sizeof(Chunk) );

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

     return newchunk;
}

static Chunk*
free_chunk( SurfaceManager *manager, Chunk *chunk )
{
     if (!chunk->buffer) {
          BUG( "freeing free chunk" );
          return chunk;
     }
     else {
          DEBUGMSG( "freeing chunk at %d\n", chunk->offset );
     }

     if (chunk->buffer->policy == CSP_VIDEOONLY)
          manager->available += chunk->length;

     chunk->buffer = NULL;

     min_toleration--;

     if (chunk->prev  &&  !chunk->prev->buffer) {
          Chunk *prev = chunk->prev;

          DEBUGMSG( "  merging with previous chunk at %d\n", prev->offset );

          prev->length += chunk->length;

          prev->next = chunk->next;
          if (prev->next)
               prev->next->prev = prev;

          DEBUGMSG("freeing %p (prev %p, next %p)\n", chunk, chunk->prev, chunk->next);

          shfree( chunk );
          chunk = prev;
     }

     if (chunk->next  &&  !chunk->next->buffer) {
          Chunk *next = chunk->next;

          DEBUGMSG( "  merging with next chunk at %d\n", next->offset );

          chunk->length += next->length;

          chunk->next = next->next;
          if (chunk->next)
               chunk->next->prev = chunk;

          shfree( next );
     }

     return chunk;
}

static void
occupy_chunk( SurfaceManager *manager, Chunk *chunk, SurfaceBuffer *buffer, int length )
{
     if (buffer->policy == CSP_VIDEOONLY)
          manager->available -= length;
     
     chunk = split_chunk( chunk, length );

     buffer->video.health = CSH_RESTORE;
     buffer->video.offset = chunk->offset;
     buffer->video.chunk = chunk;

     chunk->buffer = buffer;

     min_toleration++;

     DEBUGMSG( "DirectFB/core/surfacemanager: "
               "Allocated %d bytes at offset %d.\n",
               chunk->length, chunk->offset );
}

