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

#include <directfb.h>

#include "core.h"
#include "coredefs.h"
#include "surfacemanager.h"

#include "gfxcard.h"



static Chunk *chunks = NULL;

static int min_toleration = 1;

/*
 * helpers
 */
static inline Chunk* split_chunk( Chunk *c, int length )
{
     Chunk *newchunk;

     if (c->length == length)          /* does not need be splitted */
          return c;

     newchunk = (Chunk*) calloc( 1, sizeof(Chunk) );

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

static inline Chunk* free_chunk( Chunk *chunk )
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

          free( chunk );
          chunk = prev;
     }

     if (chunk->next  &&  !chunk->next->buffer) {
          Chunk *next = chunk->next;

          DEBUGMSG( "  merging with next chunk at %d\n", next->offset );

          chunk->length += next->length;

          chunk->next = next->next;
          if (chunk->next)
               chunk->next->prev = chunk;

          free( next );
     }

     return chunk;
}

void occupy_chunk( Chunk *chunk, SurfaceBuffer *buffer, int length )
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

DFBResult surfacemanager_suspend()
{
     Chunk *c = chunks;

     DEBUGMSG( "DirectFB/core/surfacemanager: suspending...\n" );

     while (c) {
          if (c->buffer &&
              c->buffer->policy != CSP_VIDEOONLY &&
              c->buffer->video.health == CSH_STORED)
          {
               c->buffer->video.health = CSH_RESTORE;
          }

          c = c->next;
     }

     DEBUGMSG( "DirectFB/core/surfacemanager: ...suspended\n" );

     return DFB_OK;
}

/*
 * management functions
 */
DFBResult surfacemanager_init_heap()
{
     if (chunks) {
          BUG( "reinitialization of surface manager" );
          return DFB_BUG;
     }

     chunks = (Chunk*) calloc( 1, sizeof(Chunk) );

     chunks->offset = card->heap_offset;

     /* make sure it's a multiple of the offset alignment,
        easier for further chunk splitting */
     if (card->byteoffset_align > 1) {
          chunks->offset += card->byteoffset_align - 1;
          chunks->offset -= chunks->offset % card->byteoffset_align;
     }


     chunks->length = card->framebuffer.length - chunks->offset;

     /* make sure it's a multiple of the offset alignment,
        easier for further chunk splitting */
     if (card->byteoffset_align > 1)
          chunks->length -= chunks->length % card->byteoffset_align;

     return DFB_OK;
}

DFBResult surfacemanager_adjust_heap_offset( int offset )
{
     if (card->byteoffset_align > 1) {
          offset += card->byteoffset_align - 1;
          offset -= offset % card->byteoffset_align;
     }

     if (chunks->buffer == NULL) {
          /* first chunk is free */
          if (offset < chunks->offset + chunks->length) {
               /* ok, just recalculate offset and length */
               chunks->length = chunks->offset + chunks->length - offset;
               chunks->offset = offset;
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

     card->heap_offset = offset;

     return DFB_OK;
}

DFBResult surfacemanager_allocate( SurfaceBuffer *buffer )
{
     int pitch;
     int length;
     Chunk *c;

     Chunk *best_free = NULL;
     Chunk *best_occupied = NULL;

     CoreSurface *surface = buffer->surface;

     /* calculate the required length depending on limitations */
     pitch = surface->width;
     if (card->pixelpitch_align > 1) {
          pitch += card->pixelpitch_align - 1;
          pitch -= pitch % card->pixelpitch_align;
     }

     pitch *= BYTES_PER_PIXEL(surface->format);
     length = pitch * surface->height;

     if (card->byteoffset_align > 1) {
          length += card->byteoffset_align - 1;
          length -= length % card->byteoffset_align;
     }

     buffer->video.pitch = pitch;

     /* examine chunks */
     c = chunks;
     while (c) {
          if (c->length >= length) {
               if (c->buffer  &&
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

          pthread_mutex_lock( &kicked->front_lock );
          pthread_mutex_lock( &kicked->back_lock );

          best_occupied->buffer->video.health = CSH_INVALID;
          surface_notify_listeners( kicked, CSN_VIDEO );

          best_occupied = free_chunk( best_occupied );

          pthread_mutex_unlock( &kicked->front_lock );
          pthread_mutex_unlock( &kicked->back_lock );

          DEBUGMSG( "kicked out.\n" );


          occupy_chunk( best_occupied, buffer, length );
     }
     else {
          //char *tmp;

          DEBUGMSG( "DirectFB/core/surfacemanager: "
                    "Couldn't allocate enough heap space "
                    "for video memory surface!\n" );

          /*
          tmp = malloc( card->framebuffer.length );

          memcpy( tmp, card->framebuffer.base, card->framebuffer.length );
          memset( card->framebuffer.base, 0xFF, card->framebuffer.length );
          memcpy( card->framebuffer.base, tmp, card->framebuffer.length );

          free( tmp );
          */

          /* no luck */
          return DFB_NOVIDEOMEMORY;
     }

     return DFB_OK;
}

DFBResult surfacemanager_deallocate( SurfaceBuffer *buffer )
{
     Chunk *chunk = buffer->video.chunk;

     if (buffer->video.health == CSH_INVALID)
          return DFB_OK;

     DEBUGMSG( "deallocating...\n" );

     buffer->video.health = CSH_INVALID;
     buffer->video.chunk = NULL;

     surface_notify_listeners( buffer->surface, CSN_VIDEO );

     free_chunk( chunk );

     DEBUGMSG( "deallocated.\n" );

     return DFB_OK;
}

DFBResult surfacemanager_assure_video( SurfaceBuffer *buffer )
{
     CoreSurface *surface = buffer->surface;

     switch (buffer->video.health) {
          case CSH_STORED:
               if (buffer->video.chunk)
                    buffer->video.chunk->tolerations = 0;
               return DFB_OK;
          case CSH_INVALID: {
               DFBResult ret;

               ret = surfacemanager_allocate( buffer );
               if (ret)
                    return ret;

               /* FALL THROUGH, because after successful allocation
                  the surface health is CSH_RESTORE */
          }
          case CSH_RESTORE: {
               int h = surface->height;
               char *src = buffer->system.addr;
               char *dst = card->framebuffer.base + buffer->video.offset;

               while (h--) {
                    memcpy( dst, src,
                            surface->width * BYTES_PER_PIXEL(surface->format) );
                    src += buffer->system.pitch;
                    dst += buffer->video.pitch;
               }
               buffer->video.health = CSH_STORED;
               buffer->video.chunk->tolerations = 0;
               surface_notify_listeners( surface, CSN_VIDEO );

               return DFB_OK;
          }
     }

     BUG( "unknown video instance health" );
     return DFB_BUG;
}

DFBResult surfacemanager_assure_system( SurfaceBuffer *buffer )
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
          char *src = card->framebuffer.base + buffer->video.offset;
          char *dst = buffer->system.addr;

          while (h--) {
               memcpy( dst, src,
                       surface->width * BYTES_PER_PIXEL(surface->format) );
               src += buffer->video.pitch;
               dst += buffer->system.pitch;
          }
          buffer->system.health = CSH_STORED;
          surface_notify_listeners( surface, CSN_SYSTEM );

          return DFB_OK;
     }

     BUG( "no valid surface instance" );
     return DFB_BUG;
}
