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

#include <stdlib.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>
#include <direct/messages.h>


D_DEBUG_DOMAIN( Direct_Hash, "Direct/Hash", "Hash table implementation" );


#define REMOVED  ((void *) -1)

typedef struct {
     __u32     key;
     void     *value;
} Element;

struct __D_DirectHash {
     int       magic;

     int       size;

     int       count;
     int       removed;

     Element  *elements;
};

/**************************************************************************************************/

static inline int
locate_key( const DirectHash *hash, __u32 key )
{
     int            pos;
     const Element *element;

     pos = key % hash->size;

     element = &hash->elements[pos];

     while (element->value) {
          if (element->value != REMOVED && element->key == key)
               return pos;

          if (++pos == hash->size)
               pos = 0;

          element = &hash->elements[pos];
     }

     return -1;
}

/**************************************************************************************************/

DirectResult
direct_hash_create( int          size,
                    DirectHash **ret_hash )
{
     DirectHash *hash;

     if (size < 17)
          size = 17;

     D_DEBUG_AT( Direct_Hash, "Creating hash table with initial capacity of %d...\n", size );

     hash = D_CALLOC( 1, sizeof (DirectHash) );
     if (!hash) {
          D_WARN( "out of memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     hash->size     = size;
     hash->elements = D_CALLOC( size, sizeof(Element) );

     if (!hash->elements) {
          D_WARN( "out of memory" );
          D_FREE( hash );
          return DFB_NOSYSTEMMEMORY;
     }

     D_MAGIC_SET( hash, DirectHash );

     *ret_hash = hash;

     return DFB_OK;
}

void
direct_hash_destroy( DirectHash *hash )
{
     D_MAGIC_ASSERT( hash, DirectHash );

     D_MAGIC_CLEAR( hash );

     D_FREE( hash->elements );
     D_FREE( hash );
}

DirectResult
direct_hash_insert( DirectHash *hash,
                    __u32       key,
                    void       *value )
{
     int      pos;
     Element *element;

     D_MAGIC_ASSERT( hash, DirectHash );
     D_ASSERT( value != NULL );

     /* Need to resize the hash table? */
     if ((hash->count + hash->removed) > hash->size / 4) {
          int      i, size = hash->size * 3;
          Element *elements;

          D_DEBUG_AT( Direct_Hash, "Resizing from %d to %d... (count %d, removed %d)\n",
                      hash->size, size, hash->count, hash->removed );

          elements = D_CALLOC( size, sizeof(Element) );
          if (!elements) {
               D_WARN( "out of memory" );
               return DFB_NOSYSTEMMEMORY;
          }

          for (i=0; i<hash->size; i++) {
               Element *element = &hash->elements[i];

               if (element->value && element->value != REMOVED) {
                    pos = element->key % size;

                    elements[pos] = *element;
               }
          }

          hash->size     = size;
          hash->elements = elements;
          hash->removed  = 0;
     }

     pos = key % hash->size;

     D_DEBUG_AT( Direct_Hash, "Attempting to insert key 0x%08x at position %d...\n", key, pos );

     element = &hash->elements[pos];

     while (element->value && element->value != REMOVED) {
          if (element->key == key) {
               D_BUG( "key already exists" );
               return DFB_BUG;
          }

          if (++pos == hash->size)
               pos = 0;

          element = &hash->elements[pos];
     }

     if (element->value == REMOVED)
          hash->removed--;

     element->key   = key;
     element->value = value;

     hash->count++;

     D_DEBUG_AT( Direct_Hash, "...inserted at %d, new count = %d, removed = %d, size = %d.\n",
                 pos, hash->count, hash->removed, hash->size );

     return DFB_OK;
}

void
direct_hash_remove( DirectHash  *hash,
                    __u32        key )
{
     int pos;

     D_MAGIC_ASSERT( hash, DirectHash );

     pos = locate_key( hash, key );
     if (pos == -1) {
          D_WARN( "key not found" );
          return;
     }

     hash->elements[pos].value = REMOVED;

     hash->count--;
     hash->removed++;

     D_DEBUG_AT( Direct_Hash, "Removed key 0x%08x at %d, new count = %d, removed = %d, size = %d.\n",
                 key, pos, hash->count, hash->removed, hash->size );
}

void *
direct_hash_lookup( DirectHash *hash,
                    __u32       key )
{
     int pos;

     D_MAGIC_ASSERT( hash, DirectHash );

     pos = locate_key( hash, key );

     return (pos != -1) ? hash->elements[pos].value : NULL;
}

void
direct_hash_iterate( DirectHash             *hash,
                     DirectHashIteratorFunc  func,
                     void                   *ctx )
{
     int i;

     D_MAGIC_ASSERT( hash, DirectHash );

     for (i=0; i<hash->size; i++) {
          Element *element = &hash->elements[i];

          if (!element->value || element->value == REMOVED)
               continue;

          if (!func( hash, element->key, element->value, ctx ) )
               return;
     }
}

