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

#ifndef __FUSION__VECTOR_H__
#define __FUSION__VECTOR_H__

#include "fusion_types.h"

#include <misc/debug.h>

typedef struct {
     int    magic;

     void **elements;
     int    count;

     int    capacity;
} FusionVector;

void fusion_vector_init   ( FusionVector *vector, int capacity );
void fusion_vector_destroy( FusionVector *vector );

FusionResult fusion_vector_add        ( FusionVector *vector,
                                        void         *element );

FusionResult fusion_vector_insert     ( FusionVector *vector,
                                        void         *element,
                                        int           index );

FusionResult fusion_vector_remove     ( FusionVector *vector,
                                        int           index );

FusionResult fusion_vector_remove_last( FusionVector *vector );


static inline bool
fusion_vector_empty( const FusionVector *vector )
{
     DFB_ASSERT( vector != NULL );
     DFB_MAGIC_ASSERT( vector, FusionVector );

     return vector->count == 0;
}

static inline int
fusion_vector_size( const FusionVector *vector )
{
     DFB_ASSERT( vector != NULL );
     DFB_MAGIC_ASSERT( vector, FusionVector );

     return vector->count;
}

static inline void *
fusion_vector_at( const FusionVector *vector, int index )
{
     DFB_ASSERT( vector != NULL );
     DFB_MAGIC_ASSERT( vector, FusionVector );
     DFB_ASSERT( index >= 0 );
     DFB_ASSERT( index < vector->count );

     return vector->elements[index];
}

static inline bool
fusion_vector_contains( const FusionVector *vector, void *element )
{
     int i;

     DFB_ASSERT( vector != NULL );
     DFB_MAGIC_ASSERT( vector, FusionVector );
     DFB_ASSERT( element != NULL );

     /* Start with more recently added elements. */
     for (i=vector->count-1; i>=0; i++)
          if (vector->elements[i] == element)
               return true;

     return false;
}

static inline int
fusion_vector_index_of( const FusionVector *vector, void *element )
{
     int i;

     DFB_ASSERT( vector != NULL );
     DFB_MAGIC_ASSERT( vector, FusionVector );
     DFB_ASSERT( element != NULL );

     /* Start with more recently added elements. */
     for (i=vector->count-1; i>=0; i--)
          if (vector->elements[i] == element)
               return i;

     return -1;
}


#define fusion_vector_foreach(vector, index, element)                         \
     for ((index) = 0;                                                        \
          (index) < (vector)->count && (element = (vector)->elements[index]); \
          (index)++)

#endif /* __FUSION__VECTOR_H__ */

