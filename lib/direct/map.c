/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdlib.h>

#include <direct/debug.h>
#include <direct/map.h>
#include <direct/mem.h>
#include <direct/messages.h>


D_DEBUG_DOMAIN( Direct_Map, "Direct/Map", "Map implementation" );

/**********************************************************************************************************************/

#define REMOVED  ((void *) -1)

typedef struct {
     unsigned int           hash;
     void                  *object;
} MapEntry;

struct __D_DirectMap {
     int                    magic;

     unsigned int           size;

     unsigned int           count;
     unsigned int           removed;

     MapEntry              *entries;

     DirectMapCompareFunc   compare;
     DirectMapHashFunc      hash;
     void                  *ctx;
};

#define DIRECT_MAP_ASSERT( map )             \
     do {                                    \
          D_MAGIC_ASSERT( map, DirectMap );  \
     } while (0)

/**********************************************************************************************************************/

static int
locate_entry( DirectMap *map, unsigned int hash, const void *key )
{
     D_DEBUG_AT( Direct_Map, "%s( hash %u )\n", __func__, hash );

     DIRECT_MAP_ASSERT( map );
     D_ASSERT( key != NULL );

     ///

     int             pos;
     const MapEntry *entry;

     pos = hash % map->size;

     entry = &map->entries[pos];

     while (entry->object) {
          if (entry->object != REMOVED && entry->hash == hash && map->compare( map, key, entry->object, map->ctx ))
               return pos;

          if (++pos == map->size)
               pos = 0;

          entry = &map->entries[pos];
     }

     return -1;
}

static DirectResult
resize_map( DirectMap    *map,
            unsigned int  size )
{
     D_DEBUG_AT( Direct_Map, "%s( size %u )\n", __func__, size );

     DIRECT_MAP_ASSERT( map );
     D_ASSERT( size > 3 );

     ///

     int       i, pos;
     MapEntry *entries;

     entries = D_CALLOC( size, sizeof(MapEntry) );
     if (!entries)
          return D_OOM();

     for (i=0; i<map->size; i++) {
          MapEntry *entry = &map->entries[i];
          MapEntry *insertElement;

          if (entry->object && entry->object != REMOVED) {
               pos = entry->hash % size;

               insertElement = &entries[pos];
               while (insertElement->object && insertElement->object != REMOVED) {
                   if (++pos == size)
                       pos = 0;
                   insertElement = &entries[pos];
               }

               entries[pos] = *entry;
          }
     }

     D_FREE( map->entries );

     map->size    = size;
     map->entries = entries;
     map->removed = 0;

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
direct_map_create( unsigned int           initial_size,
                   DirectMapCompareFunc   compare_func,
                   DirectMapHashFunc      hash_func,
                   void                  *ctx,
                   DirectMap            **ret_map )
{
     D_DEBUG_AT( Direct_Map, "%s( size %u, compare %p, hash %p )\n", __func__, initial_size, compare_func, hash_func );

     D_ASSERT( compare_func != NULL );
     D_ASSERT( hash_func != NULL );
     D_ASSERT( ret_map != NULL );

     ///

     DirectMap *map;

     if (initial_size < 3)
          initial_size = 3;

     map = D_CALLOC( 1, sizeof (DirectMap) );
     if (!map)
          return D_OOM();

     map->entries = D_CALLOC( initial_size, sizeof(MapEntry) );
     if (!map->entries) {
          D_FREE( map );
          return D_OOM();
     }

     map->size    = initial_size;
     map->compare = compare_func;
     map->hash    = hash_func;
     map->ctx     = ctx;

     D_MAGIC_SET( map, DirectMap );

     *ret_map = map;

     return DR_OK;
}

void
direct_map_destroy( DirectMap *map )
{
     D_DEBUG_AT( Direct_Map, "%s()\n", __func__ );

     DIRECT_MAP_ASSERT( map );

     ///

     D_MAGIC_CLEAR( map );

     D_FREE( map->entries );
     D_FREE( map );
}

DirectResult
direct_map_insert( DirectMap  *map,
                   const void *key,
                   void       *object )
{
     D_DEBUG_AT( Direct_Map, "%s( key %p, object %p )\n", __func__, key, object );

     DIRECT_MAP_ASSERT( map );
     D_ASSERT( key != NULL );
     D_ASSERT( object != NULL );

     ///

     /* Need to resize the map? */
     if ((map->count + map->removed) > map->size / 4)
          resize_map( map, map->size * 3 );


     unsigned int hash = map->hash( map, key, map->ctx );
     int          pos  = hash % map->size;

     D_DEBUG_AT( Direct_Map, "  -> hash %u, pos %d\n", hash, pos );


     MapEntry *entry = &map->entries[pos];

     while (entry->object && entry->object != REMOVED) {
          if (entry->hash == hash && map->compare( map, key, entry->object, map->ctx )) {
               if (entry->object == object) {
                    D_DEBUG_AT( Direct_Map, "  -> same object with matching key already exists\n" );
                    return DR_BUSY;
               }
               else {
                    D_DEBUG_AT( Direct_Map, "  -> different object with matching key already exists\n" );
                    D_BUG( "different object with matching key already exists" );
                    return DR_BUG;
               }
          }

          if (++pos == map->size)
               pos = 0;

          entry = &map->entries[pos];
     }

     if (entry->object == REMOVED)
          map->removed--;

     entry->hash   = hash;
     entry->object = object;

     map->count++;

     D_DEBUG_AT( Direct_Map, "  -> new count = %d, removed = %d, size = %d\n", map->count, map->removed, map->size );

     return DR_OK;
}

DirectResult
direct_map_remove( DirectMap  *map,
                   const void *key )
{
     D_DEBUG_AT( Direct_Map, "%s( key %p )\n", __func__, key );

     DIRECT_MAP_ASSERT( map );
     D_ASSERT( key != NULL );

     ///

     unsigned int hash = map->hash( map, key, map->ctx );
     int          pos;

     pos = locate_entry( map, hash, key );
     if (pos == -1) {
          D_WARN( "object to remove not found" );
          return DR_ITEMNOTFOUND;
     }

     map->entries[pos].object = REMOVED;

     map->count--;
     map->removed++;

     D_DEBUG_AT( Direct_Map, "  -> new count = %d, removed = %d, size = %d\n", map->count, map->removed, map->size );

     return DR_OK;
}

void *
direct_map_lookup( DirectMap  *map,
                   const void *key )
{
     D_DEBUG_AT( Direct_Map, "%s( key %p )\n", __func__, key );

     DIRECT_MAP_ASSERT( map );
     D_ASSERT( key != NULL );

     ///

     unsigned int hash = map->hash( map, key, map->ctx );
     int          pos;

     pos = locate_entry( map, hash, key );

     return (pos != -1) ? map->entries[pos].object : NULL;
}

void
direct_map_iterate( DirectMap             *map,
                    DirectMapIteratorFunc  func,
                    void                  *ctx )
{
     D_DEBUG_AT( Direct_Map, "%s( func %p )\n", __func__, func );

     DIRECT_MAP_ASSERT( map );
     D_ASSERT( func != NULL );

     ///

     int i;

     for (i=0; i<map->size; i++) {
          MapEntry *entry = &map->entries[i];

          if (entry->object && entry->object != REMOVED) {
               if (func( map, entry->object, ctx ) != DENUM_OK)
                    break;
          }
     }
}

