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

#include <direct/debug.h>
#include <direct/memcpy.h>

#include <fusion/object.h>
#include <fusion/shmalloc.h>
#include <fusion/vector.h>


static inline bool ensure_capacity( FusionVector *vector )
{
     D_MAGIC_ASSERT( vector, FusionVector );
     D_ASSERT( vector->capacity > 0 );

     if (!vector->elements) {
          vector->elements = SHMALLOC( vector->capacity * sizeof(void*) );
          if (!vector->elements)
               return false;
     }
     else if (vector->count == vector->capacity) {
          void *elements;
          void *oldelements = vector->elements;
          int   capacity    = vector->capacity << 1;

          elements = SHMALLOC( capacity * sizeof(void*) );
          if (!elements)
               return false;

          direct_memcpy( elements, vector->elements,
                         vector->count * sizeof(void*) );

          vector->elements = elements;
          vector->capacity = capacity;

          SHFREE( oldelements );
     }

     return true;
}

void
fusion_vector_init( FusionVector *vector, int capacity )
{
     D_ASSERT( vector != NULL );
     D_ASSERT( capacity > 0 );

     vector->elements = NULL;
     vector->count    = 0;
     vector->capacity = capacity;

     D_MAGIC_SET( vector, FusionVector );
}

void
fusion_vector_destroy( FusionVector *vector )
{
     D_MAGIC_ASSERT( vector, FusionVector );
     D_ASSERT( vector->count == 0 || vector->elements != NULL );

     if (vector->elements) {
          SHFREE( vector->elements );
          vector->elements = NULL;
     }

     D_MAGIC_CLEAR( vector );
}

DirectResult
fusion_vector_add( FusionVector *vector,
                   void         *element )
{
     D_MAGIC_ASSERT( vector, FusionVector );
     D_ASSERT( element != NULL );

     /* Make sure there's a free entry left. */
     if (!ensure_capacity( vector ))
          return DFB_NOSYSTEMMEMORY;

     /* Add the element to the vector. */
     vector->elements[vector->count++] = element;

     return DFB_OK;
}

DirectResult
fusion_vector_insert( FusionVector *vector,
                      void         *element,
                      int           index )
{
     D_MAGIC_ASSERT( vector, FusionVector );
     D_ASSERT( element != NULL );
     D_ASSERT( index >= 0 );
     D_ASSERT( index <= vector->count );

     /* Make sure there's a free entry left. */
     if (!ensure_capacity( vector ))
          return DFB_NOSYSTEMMEMORY;

     /* Move elements from insertion point one up. */
     memmove( &vector->elements[ index + 1 ],
              &vector->elements[ index ],
              (vector->count - index) * sizeof(void*) );

     /* Insert the element into the vector. */
     vector->elements[index] = element;

     /* Increase the element counter. */
     vector->count++;

     return DFB_OK;
}

DirectResult
fusion_vector_move( FusionVector *vector,
                    int           from,
                    int           to )
{
     void *element;

     D_MAGIC_ASSERT( vector, FusionVector );
     D_ASSERT( from >= 0 );
     D_ASSERT( from < vector->count );
     D_ASSERT( to >= 0 );
     D_ASSERT( to < vector->count );

     if (to == from)
          return DFB_OK;

     /* Save the element. */
     element = vector->elements[from];

     /* Move elements that lie on the way to the new position. */
     if (to > from) {
          /* Element is moving up -> move other elements down. */
          memmove( &vector->elements[ from ],
                   &vector->elements[ from + 1 ],
                   (to - from) * sizeof(void*) );
     }
     else {
          /* Element is moving down -> move other elements up. */
          memmove( &vector->elements[ to + 1 ],
                   &vector->elements[ to ],
                   (from - to) * sizeof(void*) );
     }

     /* Restore the element at the new position. */
     vector->elements[to] = element;

     return DFB_OK;
}

DirectResult
fusion_vector_remove( FusionVector *vector,
                      int           index )
{
     D_MAGIC_ASSERT( vector, FusionVector );
     D_ASSERT( index >= 0 );
     D_ASSERT( index < vector->count );

     /* Move elements after this element one down. */
     memmove( &vector->elements[ index ],
              &vector->elements[ index + 1 ],
              (vector->count - index - 1) * sizeof(void*) );

     /* Decrease the element counter. */
     vector->count--;

     return DFB_OK;
}

DirectResult
fusion_vector_remove_last( FusionVector *vector )
{
     D_MAGIC_ASSERT( vector, FusionVector );
     D_ASSERT( vector->count > 0 );

     /* Decrease the element counter. */
     vector->count--;

     return DFB_OK;
}

