/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <malloc.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include "directfb.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"

#include "gfxcard.h"
#include "surfaces.h"
#include "surfacemanager.h"

#include "misc/util.h"
#include "misc/mem.h"

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

     CoreSurface    *surfaces;
     Chunk          *chunks;

     /* offset of the surface heap */
     unsigned int    heap_offset;

     /* card limitations for surface offsets and their pitch */
     unsigned int    byteoffset_align;
     unsigned int    pixelpitch_align;
};


static int min_toleration = 1;

static Chunk* split_chunk( Chunk *c, int length );
static Chunk* free_chunk( Chunk *chunk );
static void occupy_chunk( Chunk *chunk, SurfaceBuffer *buffer, int length );


SurfaceManager *surfacemanager_create( unsigned int length,
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
          shmfree( manager );
          return NULL;
     }

     chunk->offset = 0;
     chunk->length = length;

     manager->chunks           = chunk;
     manager->byteoffset_align = byteoffset_align;
     manager->pixelpitch_align = pixelpitch_align;

     skirmish_init( &manager->lock );

     return manager;
}

void surfacemanager_lock( SurfaceManager *manager )
{
     skirmish_prevail( &manager->lock );
}

void surfacemanager_unlock( SurfaceManager *manager )
{
     skirmish_dismiss( &manager->lock );
}

/** public functions locking the surfacemanger theirself,
    NOT to be called between lock/unlock of surfacemanager **/

void surfacemanager_add_surface( SurfaceManager *manager,
                                 CoreSurface    *surface )
{
     surfacemanager_lock( manager );

     surface->manager = manager;

     if (!manager->surfaces) {
          surface->next = NULL;
          surface->prev = NULL;
          manager->surfaces = surface;
     }
     else {
          surface->prev = NULL;
          surface->next = manager->surfaces;
          manager->surfaces->prev = surface;
          manager->surfaces = surface;
     }

     surfacemanager_unlock( manager );
}

void surfacemanager_remove_surface( SurfaceManager *manager,
                                    CoreSurface    *surface )
{
     surfacemanager_lock( manager );

     if (surface->prev)
          surface->prev->next = surface->next;
     else
          manager->surfaces = surface->next;

     if (surface->next)
          surface->next->prev = surface->prev;

     surfacemanager_unlock( manager );
}


DFBResult surfacemanager_adjust_heap_offset( SurfaceManager *manager,
                                             unsigned int    offset )
{
     surfacemanager_lock( manager );

     if (manager->byteoffset_align > 1) {
          offset += manager->byteoffset_align - 1;
          offset -= offset % manager->byteoffset_align;
     }

     if (manager->chunks->buffer == NULL) {
          /* first chunk is free */
          if (offset < manager->chunks->offset + manager->chunks->length) {
               /* ok, just recalculate offset and length */
               manager->chunks->length = manager->chunks->offset + manager->chunks->length - offset;
               manager->chunks->offset = offset;
          }
          else {
               /* more space needed than free at the beginning */
               /* TODO: move/destroy instances */
          }
     }
     else {
          /* very rare case that the first chunk is occupied */
          /* TODO: move/destroy instances */
     }

     manager->heap_offset = offset;

     surfacemanager_unlock( manager );

     return DFB_OK;
}

/** public functions NOT locking the surfacemanger theirself,
    to be called between lock/unlock of surfacemanager **/

DFBResult surfacemanager_allocate( SurfaceManager *manager,
                                   SurfaceBuffer  *buffer )
{
     int pitch;
     int length;
     Chunk *c;

     Chunk *best_free = NULL;
     Chunk *best_occupied = NULL;

     CoreSurface *surface = buffer->surface;

     /* calculate the required length depending on limitations */
     pitch = surface->width;
     if (manager->pixelpitch_align > 1) {
          pitch += manager->pixelpitch_align - 1;
          pitch -= pitch % manager->pixelpitch_align;
     }

     pitch *= DFB_BYTES_PER_PIXEL(surface->format);
     length = pitch * surface->height;

     if (manager->byteoffset_align > 1) {
          length += manager->byteoffset_align - 1;
          length -= length % manager->byteoffset_align;
     }

     buffer->video.pitch = pitch;

     /* examine chunks */
     c = manager->chunks;
     while (c) {
          if (c->length >= length) {
               if (c->buffer  &&
                   !c->buffer->video.locked &&
                   c->buffer->policy != CSP_VIDEOONLY  &&
                   ((c->tolerations > min_toleration) ||
                   buffer->policy == CSP_VIDEOONLY))
               {
                    /* found a nice place to chill */
                    if (!best_occupied  ||
                         best_occupied->length > c->length  ||
                         best_occupied->tolerations < c->tolerations)
                         /* first found or better one? */
                         best_occupied = c;
               } else
               if (!c->buffer) {
                    /* found a nice place to chill */
                    if (!best_free  ||  best_free->length > c->length)
                         /* first found or better one? */
                         best_free = c;
               } else
                    c->tolerations++;
          }

          c = c->next;
     }

     /* if we found a place */
     if (best_free) {
          occupy_chunk( best_free, buffer, length );
     } else
     if (best_occupied) {
          CoreSurface *kicked = best_occupied->buffer->surface;

          DEBUGMSG( "kicking out surface at %d with tolerations %d...\n",
                    best_occupied->offset, best_occupied->tolerations );

          surfacemanager_assure_system( manager, best_occupied->buffer );

          best_occupied->buffer->video.health = CSH_INVALID;
          surface_notify_listeners( kicked, CSNF_VIDEO );

          best_occupied = free_chunk( best_occupied );

          DEBUGMSG( "kicked out.\n" );


          occupy_chunk( best_occupied, buffer, length );
     }
     else {
          //char *tmp;

          DEBUGMSG( "DirectFB/core/surfacemanager: "
                    "Couldn't allocate enough heap space "
                    "for video memory surface!\n" );

          /*
          tmp = DFBMALLOC( card->framebuffer.length );

          memcpy( tmp, card->framebuffer.base, card->framebuffer.length );
          memset( card->framebuffer.base, 0xFF, card->framebuffer.length );
          memcpy( card->framebuffer.base, tmp, card->framebuffer.length );

          DFBFREE( tmp );
          */

          /* no luck */
          return DFB_NOVIDEOMEMORY;
     }

     return DFB_OK;
}

DFBResult surfacemanager_deallocate( SurfaceManager *manager,
                                     SurfaceBuffer  *buffer )
{
     Chunk *chunk = buffer->video.chunk;

     if (buffer->video.health == CSH_INVALID)
          return DFB_OK;

     DEBUGMSG( "deallocating...\n" );

     buffer->video.health = CSH_INVALID;
     buffer->video.chunk = NULL;

     free_chunk( chunk );

     surface_notify_listeners( buffer->surface, CSNF_VIDEO );

     DEBUGMSG( "deallocated.\n" );

     return DFB_OK;
}

DFBResult surfacemanager_assure_video( SurfaceManager *manager,
                                       SurfaceBuffer  *buffer )
{
     CoreSurface *surface = buffer->surface;

     switch (buffer->video.health) {
          case CSH_STORED:
               if (buffer->video.chunk)
                    buffer->video.chunk->tolerations = 0;
               return DFB_OK;
          case CSH_INVALID: {
               DFBResult ret;

               ret = surfacemanager_allocate( manager, buffer );
               if (ret)
                    return ret;

               /* FALL THROUGH, because after successful allocation
                  the surface health is CSH_RESTORE */
          }
          case CSH_RESTORE: {
               int h = surface->height;
               char *src = buffer->system.addr;
               char *dst = gfxcard_memory_virtual( buffer->video.offset );

               if (buffer->system.health != CSH_STORED)
                    BUG( "system/video instances both not stored!" );

               while (h--) {
                    memcpy( dst, src,
                            surface->width * DFB_BYTES_PER_PIXEL(surface->format) );
                    src += buffer->system.pitch;
                    dst += buffer->video.pitch;
               }
               buffer->video.health = CSH_STORED;
               buffer->video.chunk->tolerations = 0;
               surface_notify_listeners( surface, CSNF_VIDEO );

               return DFB_OK;
          }
     }

     BUG( "unknown video instance health" );
     return DFB_BUG;
}

DFBResult surfacemanager_assure_system( SurfaceManager *manager,
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
          int h = surface->height;
          char *src = gfxcard_memory_virtual( buffer->video.offset );
          char *dst = buffer->system.addr;

          while (h--) {
               memcpy( dst, src,
                       surface->width * DFB_BYTES_PER_PIXEL(surface->format) );
               src += buffer->video.pitch;
               dst += buffer->system.pitch;
          }
          buffer->system.health = CSH_STORED;
          surface_notify_listeners( surface, CSNF_SYSTEM );

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

static Chunk* free_chunk( Chunk *chunk )
{
     if (!chunk->buffer) {
          BUG( "freeing free chunk" );
          return chunk;
     }
     else {
          DEBUGMSG( "freeing chunk at %d\n", chunk->offset );
     }

     chunk->buffer = NULL;

     //min_toleration--;

     if (chunk->prev  &&  !chunk->prev->buffer) {
          Chunk *prev = chunk->prev;

          DEBUGMSG( "  merging with previous chunk at %d\n", prev->offset );

          prev->length += chunk->length;

          prev->next = chunk->next;
          if (prev->next)
               prev->next->prev = prev;

          DEBUGMSG("freeing %p (prev %p, next %p)\n", chunk, chunk->prev, chunk->next);

          shmfree( chunk );
          chunk = prev;
     }

     if (chunk->next  &&  !chunk->next->buffer) {
          Chunk *next = chunk->next;

          DEBUGMSG( "  merging with next chunk at %d\n", next->offset );

          chunk->length += next->length;

          chunk->next = next->next;
          if (chunk->next)
               chunk->next->prev = chunk;

          shmfree( next );
     }

     return chunk;
}

static void occupy_chunk( Chunk *chunk, SurfaceBuffer *buffer, int length )
{
     chunk = split_chunk( chunk, length );

     buffer->video.health = CSH_RESTORE;
     buffer->video.offset = chunk->offset;
     buffer->video.chunk = chunk;

     chunk->buffer = buffer;

     //min_toleration++;

     DEBUGMSG( "DirectFB/core/surfacemanager: "
               "Allocated %d bytes at offset %d.\n",
               chunk->length, chunk->offset );
}

