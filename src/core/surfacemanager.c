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

#include <malloc.h>

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

#include "misc/util.h"
#include "misc/mem.h"
#include "misc/memcpy.h"
#include "misc/fbdebug.h"

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

     struct {
          unsigned int    width;
          unsigned int    height;
          unsigned int    total;

          FBDebugArea    *area;
     } debug;

     /* offset of the surface heap */
     unsigned int    heap_offset;

     /* card limitations for surface offsets and their pitch */
     unsigned int    byteoffset_align;
     unsigned int    pixelpitch_align;
};


static int min_toleration = 8;

static Chunk* split_chunk( Chunk *c, int length );
static Chunk* free_chunk( Chunk *chunk );
static void occupy_chunk( Chunk *chunk, SurfaceBuffer *buffer, int length );

#ifdef DFB_DEBUG
static void debug_init( SurfaceManager *manager );
static void debug_exit( SurfaceManager *manager );
static void debug_linear_fill( SurfaceManager *manager,
                               int start, int length, __u8 r, __u8 g, __u8 b );
static void debug_dump( SurfaceManager *manager );
static void debug_pause( SurfaceManager *manager );
#endif


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
     manager->byteoffset_align = byteoffset_align;
     manager->pixelpitch_align = pixelpitch_align;

     skirmish_init( &manager->lock );

#ifdef DFB_DEBUG
     debug_init( manager );
#endif

     return manager;
}

void
dfb_surfacemanager_destroy( SurfaceManager *manager )
{
     Chunk *chunk;

     DFB_ASSERT( manager != NULL );
     DFB_ASSERT( manager->chunks != NULL );

#ifdef DFB_DEBUG
     debug_exit( manager );
#endif
     
     /* Deallocate all chunks. */
     chunk = manager->chunks;
     while (chunk) {
          Chunk *next = chunk->next;

          shfree( chunk );

          chunk = next;
     }

     /* Destroy manager lock. */
     skirmish_destroy( &manager->lock );
     
     /* Deallocate manager struct. */
     shfree( manager );
}

#ifdef FUSION_FAKE
DFBResult dfb_surfacemanager_suspend( SurfaceManager *manager )
{
     Chunk *c;

     dfb_surfacemanager_lock( manager );

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
     return DFB_OK;
}
#endif

void dfb_surfacemanager_lock( SurfaceManager *manager )
{
     skirmish_prevail( &manager->lock );
}

void dfb_surfacemanager_unlock( SurfaceManager *manager )
{
     skirmish_dismiss( &manager->lock );
}

DFBResult dfb_surfacemanager_adjust_heap_offset( SurfaceManager *manager,
                                                 unsigned int    offset )
{
     dfb_surfacemanager_lock( manager );

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

#ifdef DFB_DEBUG
     debug_dump( manager );
#endif

     dfb_surfacemanager_unlock( manager );

     return DFB_OK;
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

     buffer->video.pitch = pitch;

     /* examine chunks */
     c = manager->chunks;
     while (c) {
          if (c->length >= length) {
               if (c->buffer  &&
                   !c->buffer->video.locked &&
                   c->buffer->policy != CSP_VIDEOONLY  &&
                   ((c->tolerations > min_toleration/8) ||
                   buffer->policy == CSP_VIDEOONLY))
               {
                    /* found a nice place to chill */
                    if (!best_occupied  ||
                         best_occupied->length > c->length  ||
                         best_occupied->tolerations < c->tolerations)
                         /* first found or better one? */
                         best_occupied = c;

                    c->tolerations++;
               } else
               if (!c->buffer) {
                    /* found a nice place to chill */
                    if (!best_free  ||  best_free->length > c->length)
                         /* first found or better one? */
                         best_free = c;
               } else
                    c->tolerations++;
          } else
          if (c->buffer)
               c->tolerations++;

          c = c->next;
     }

     /* if we found a place */
     if (best_free) {
          /*
             debug_linear_fill( manager, best_free->offset,
                                best_free->length, 0x90, 0x90, 0x90 );
             debug_pause( manager );
          */

          occupy_chunk( best_free, buffer, length );
     } else
     if (best_occupied) {
          CoreSurface *kicked = best_occupied->buffer->surface;

          DEBUGMSG( "kicking out surface at %d with tolerations %d...\n",
                    best_occupied->offset, best_occupied->tolerations );

#ifdef DFB_DEBUG
          debug_linear_fill( manager, best_occupied->offset,
                             best_occupied->length, 0xff, 0xff, 0xff );

          debug_pause( manager );
#endif

          dfb_surfacemanager_assure_system( manager, best_occupied->buffer );

          best_occupied->buffer->video.health = CSH_INVALID;
          dfb_surface_notify_listeners( kicked, CSNF_VIDEO );

          best_occupied = free_chunk( best_occupied );

          dfb_gfxcard_sync();

          DEBUGMSG( "kicked out.\n" );


          occupy_chunk( best_occupied, buffer, length );
     }
     else {
          DEBUGMSG( "DirectFB/core/surfacemanager: "
                    "Couldn't allocate enough heap space "
                    "for video memory surface!\n" );

#ifdef DFB_DEBUG
          debug_linear_fill( manager, manager->heap_offset,
                             manager->length - manager->heap_offset,
                             0x20, 0x10, 0x40 );

          debug_pause( manager );

          debug_dump( manager );
#endif

          /* no luck */
          return DFB_NOVIDEOMEMORY;
     }

#ifdef DFB_DEBUG
     debug_dump( manager );
#endif

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

#ifdef DFB_DEBUG
     debug_dump( manager );
#endif

     dfb_surface_notify_listeners( buffer->surface, CSNF_VIDEO );

     while (buffer->video.locked) {
          if (++loops > 1000)
               break;
          
          sched_yield();
     }

     if (chunk)
          free_chunk( chunk );
     
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
#ifdef DFB_DEBUG
                    debug_dump( manager );
#endif
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
               int   h   = DFB_PLANE_MULTIPLY(surface->format, surface->height);
               char *src = buffer->system.addr;
               char *dst = dfb_gfxcard_memory_virtual( buffer->video.offset );

               if (buffer->system.health != CSH_STORED)
                    BUG( "system/video instances both not stored!" );

               while (h--) {
                    dfb_memcpy( dst, src, DFB_BYTES_PER_LINE(surface->format,
                                                             surface->width) );
                    src += buffer->system.pitch;
                    dst += buffer->video.pitch;
               }
               buffer->video.health = CSH_STORED;
               buffer->video.chunk->tolerations = 0;
               dfb_surface_notify_listeners( surface, CSNF_VIDEO );

#ifdef DFB_DEBUG
               debug_dump( manager );
#endif

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
          int   h   = DFB_PLANE_MULTIPLY(surface->format, surface->height);
          char *src = dfb_gfxcard_memory_virtual( buffer->video.offset );
          char *dst = buffer->system.addr;

          while (h--) {
               dfb_memcpy( dst, src, DFB_BYTES_PER_LINE(surface->format,
                                                        surface->width) );
               src += buffer->video.pitch;
               dst += buffer->system.pitch;
          }
          buffer->system.health = CSH_STORED;

#ifdef DFB_DEBUG
          debug_dump( manager );
#endif

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

static void occupy_chunk( Chunk *chunk, SurfaceBuffer *buffer, int length )
{
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


#ifdef DFB_DEBUG

static void debug_init( SurfaceManager *manager )
{
     unsigned int width, height;

     fbdebug_get_size( &width, &height );
     fbdebug_get_area( 0, 0, width, height, &manager->debug.area );

     manager->debug.width  = width;
     manager->debug.height = height;
     manager->debug.total  = width * height;
}

static void debug_exit( SurfaceManager *manager )
{
     fbdebug_free_area( manager->debug.area );
}

static void debug_linear_fill( SurfaceManager *manager,
                               int start, int length, __u8 r, __u8 g, __u8 b )
{
     int s, l, x, y, w = 1;

     s = start  * (long long)manager->debug.total / manager->length;
     l = length * (long long)manager->debug.total / manager->length;

     x = s % manager->debug.width;
     y = s / manager->debug.width;

     while (l) {
          w = l;

          if (x + w > manager->debug.width)
               w = manager->debug.width - x;

          fbdebug_fill( manager->debug.area, x, y, w, 1, r, g, b );

          if (w < l) {
               x = 0;
               y++;
          }

          l -= w;
     }

     fbdebug_fill( manager->debug.area, x+w-1, y, 1, 1, 0xff, 0xff, 0xff );
}

static void debug_dump( SurfaceManager *manager )
{
     Chunk *c;

     debug_linear_fill( manager, 0, manager->heap_offset, 0, 0, 0 );

     c = manager->chunks;
     while (c) {
          if (c->buffer) {
               __u8 r = 0, g = 0, b = 0;

               switch (c->buffer->policy) {
                    case CSP_VIDEOLOW:
                         r = 0x60;
                         break;
                    case CSP_VIDEOHIGH:
                         r = 0x90;
                         break;
                    case CSP_VIDEOONLY:
                         r = 0xFF;
                         break;
                    default:
                         ;
               }

               if (c->tolerations > min_toleration/8)
                    b = r/2;

               if (c->buffer->system.health == CSH_RESTORE)
                    g = r/2;

               debug_linear_fill( manager, c->offset, c->length, r, g, b );
          }
          else
               debug_linear_fill( manager, c->offset, c->length, 0x10, 0x10, 0x18 );

          c = c->next;
     }

     debug_pause( manager );
}

static void debug_pause( SurfaceManager *manager )
{
     //usleep( 200000 );
}

#endif

