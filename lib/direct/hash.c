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

#include <config.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>
#include <direct/messages.h>


D_LOG_DOMAIN( Direct_Hash, "Direct/Hash", "Hash table implementation" );

/**********************************************************************************************************************/

static __inline__ int
locate_key( const DirectHash *hash, unsigned long key )
{
     int                      pos;
     const DirectHashElement *element;

     D_MAGIC_ASSERT( hash, DirectHash );
     D_ASSERT( hash->size > 0 );
     D_ASSERT( hash->Elements != NULL );

     pos = key % hash->size;

     element = &hash->Elements[pos];

     while (element->value) {
          if (element->value != DIRECT_HASH_ELEMENT_REMOVED && element->key == key)
               return pos;

          if (++pos == hash->size)
               pos = 0;

          element = &hash->Elements[pos];
     }

     return -1;
}

/**********************************************************************************************************************/

DirectResult
direct_hash_create( int          size,
                    DirectHash **ret_hash )
{
     DirectHash *hash;

     hash = D_CALLOC( 1, sizeof (DirectHash) );
     if (!hash)
          return D_OOM();

     direct_hash_init( hash, size );

     *ret_hash = hash;

     return DR_OK;
}

void
direct_hash_destroy( DirectHash *hash )
{
     D_MAGIC_ASSERT( hash, DirectHash );

     direct_hash_deinit( hash );

     D_FREE( hash );
}

/**********************************************************************************************************************/

void
direct_hash_init( DirectHash *hash,
                  int         size )
{
     if (size < 17)
          size = 17;

     D_DEBUG_AT( Direct_Hash, "Creating hash table with initial capacity of %d...\n", size );

     hash->size     = size;
     hash->Elements = NULL;

     D_MAGIC_SET( hash, DirectHash );
}

void
direct_hash_deinit( DirectHash *hash )
{
     D_MAGIC_ASSERT( hash, DirectHash );

     D_MAGIC_CLEAR( hash );

     if (hash->Elements) {
          if (hash->disable_debugging_alloc)
               direct_free( hash->Elements );
          else
               D_FREE( hash->Elements );

          hash->Elements = NULL;
     }
}

/**********************************************************************************************************************/

int
direct_hash_count( DirectHash *hash )
{
     D_MAGIC_ASSERT( hash, DirectHash );

     return hash->count;
}

DirectResult
direct_hash_insert( DirectHash    *hash,
                    unsigned long  key,
                    void          *value )
{
     int                pos;
     DirectHashElement *element;

     D_MAGIC_ASSERT( hash, DirectHash );
     D_ASSERT( hash->size > 0 );
     D_ASSERT( value != NULL );

     if (!hash->Elements) {
          if (hash->disable_debugging_alloc)
               hash->Elements = direct_calloc( hash->size, sizeof(DirectHashElement) );
          else
               hash->Elements = D_CALLOC( hash->size, sizeof(DirectHashElement) );

          if (!hash->Elements)
               return D_OOM();
     }

     /* Need to resize the hash table? */
     if ((hash->count + hash->removed) > hash->size / 2) {
          int                i, size = hash->size * 3;
          DirectHashElement *elements;

          D_DEBUG_AT( Direct_Hash, "Resizing from %d to %d... (count %d, removed %d)\n",
                      hash->size, size, hash->count, hash->removed );

          if (hash->disable_debugging_alloc)
               elements = direct_calloc( size, sizeof(DirectHashElement) );
          else
               elements = D_CALLOC( size, sizeof(DirectHashElement) );
          if (!elements) {
               D_WARN( "out of memory" );
               return DR_NOLOCALMEMORY;
          }

          for (i=0; i<hash->size; i++) {
               DirectHashElement *element = &hash->Elements[i];
               DirectHashElement *insertElement;

               if (element->value && element->value != DIRECT_HASH_ELEMENT_REMOVED) {
                    pos = element->key % size;

                    insertElement = &elements[pos];
                    while (insertElement->value && insertElement->value != DIRECT_HASH_ELEMENT_REMOVED) {
                        if (++pos == size)
                            pos = 0;
                        insertElement = &elements[pos];
                    }

                    elements[pos] = *element;
               }
          }

          if (hash->disable_debugging_alloc)
               direct_free( hash->Elements );
          else
               D_FREE( hash->Elements );

          hash->size     = size;
          hash->Elements = elements;
          hash->removed  = 0;
     }

     pos = key % hash->size;

     D_DEBUG_AT( Direct_Hash, "Attempting to insert key 0x%08lx at position %d...\n", key, pos );

     element = &hash->Elements[pos];

     while (element->value && element->value != DIRECT_HASH_ELEMENT_REMOVED) {
          if (element->key == key) {
               D_BUG( "key already exists" );
               return DR_BUG;
          }

          if (++pos == hash->size)
               pos = 0;

          element = &hash->Elements[pos];
     }

     if (element->value == DIRECT_HASH_ELEMENT_REMOVED)
          hash->removed--;

     element->key   = key;
     element->value = value;

     hash->count++;

     D_DEBUG_AT( Direct_Hash, "...inserted at %d, new count = %d, removed = %d, size = %d, key = 0x%08lx.\n",
                 pos, hash->count, hash->removed, hash->size, key );

     return DR_OK;
}

DirectResult
direct_hash_remove( DirectHash    *hash,
                    unsigned long  key )
{
     int pos;

     D_MAGIC_ASSERT( hash, DirectHash );

     if (!hash->Elements)
          return DR_BUFFEREMPTY;

     pos = locate_key( hash, key );
     if (pos == -1) {
          D_WARN( "key not found" );
          return DR_ITEMNOTFOUND;
     }

     hash->Elements[pos].value = DIRECT_HASH_ELEMENT_REMOVED;

     hash->count--;
     hash->removed++;

     D_DEBUG_AT( Direct_Hash, "Removed key 0x%08lx at %d, new count = %d, removed = %d, size = %d.\n",
                 key, pos, hash->count, hash->removed, hash->size );

//     direct_futex_wake( &hash->count, INT_MAX );  // FIXME: only wake if waiting

     return DR_OK;
}

void *
direct_hash_lookup( const DirectHash *hash,
                    unsigned long     key )
{
     int pos;

     D_MAGIC_ASSERT( hash, DirectHash );

     if (!hash->Elements)
          return NULL;

     pos = locate_key( hash, key );

     return (pos != -1) ? hash->Elements[pos].value : NULL;
}

void
direct_hash_iterate( DirectHash             *hash,
                     DirectHashIteratorFunc  func,
                     void                   *ctx )
{
     int i;

     D_MAGIC_ASSERT( hash, DirectHash );

     if (!hash->Elements)
          return;

     for (i=0; i<hash->size; i++) {
          DirectHashElement *element = &hash->Elements[i];

          if (!element->value || element->value == DIRECT_HASH_ELEMENT_REMOVED)
               continue;

          if (!func( hash, element->key, element->value, ctx ) )
               return;
     }
}

