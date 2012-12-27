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

#ifndef __DIRECT__HASH_H__
#define __DIRECT__HASH_H__

#include <direct/types.h>

/**********************************************************************************************************************/

typedef struct {
     unsigned long  key;
     void          *value;
} DirectHashElement;

#define DIRECT_HASH_ELEMENT_REMOVED  ((void *) -1)


struct __D_DirectHash {
     int                 magic;

     int                 size;

     int                 count;
     int                 removed;

     DirectHashElement  *Elements;

     bool                disable_debugging_alloc;
};

/**********************************************************************************************************************/

#define DIRECT_HASH_INIT( __size, __disable_debugging_alloc )    \
     {                                                           \
          0x0b161321,                                            \
          (__size < 17 ? 17 : __size),                           \
          0,                                                     \
          0,                                                     \
          NULL,                                                  \
          __disable_debugging_alloc                              \
     }

/* Hmm, not constant?      .magic    = D_MAGIC( "DirectHash" ), */

/**********************************************************************************************************************/

#define DIRECT_HASH_ASSERT( hash )                                                                       \
     do {                                                                                                \
          D_MAGIC_ASSERT( hash, DirectHash );                                                            \
          D_ASSERT( (hash)->size > 0 );                                                                  \
          D_ASSERT( (hash)->Elements != NULL || (hash)->count == 0 );                                    \
          D_ASSERT( (hash)->Elements != NULL || (hash)->removed == 0 );                                  \
          D_ASSERT( (hash)->count + (hash)->removed < (hash)->size );                                    \
     } while (0)

/**********************************************************************************************************************/

/*
 * Hash iteration callback, return false to abort iteration.
 */
typedef bool (*DirectHashIteratorFunc)( DirectHash    *hash,
                                        unsigned long  key,
                                        void          *value,
                                        void          *ctx );

/*********************************************************************************************************************** 
** Full create including allocation ...
*/

DirectResult DIRECT_API  direct_hash_create ( int                     size,
                                              DirectHash            **ret_hash );

void         DIRECT_API  direct_hash_destroy( DirectHash             *hash );

/*********************************************************************************************************************** 
** ... or just initialization of static data...
*/

void         DIRECT_API  direct_hash_init   ( DirectHash             *hash,
                                              int                     size );

void         DIRECT_API  direct_hash_deinit ( DirectHash             *hash );

/**********************************************************************************************************************/

int          DIRECT_API  direct_hash_count  ( DirectHash             *hash );

DirectResult DIRECT_API  direct_hash_insert ( DirectHash             *hash,
                                              unsigned long           key,
                                              void                   *value );

DirectResult DIRECT_API  direct_hash_remove ( DirectHash             *hash,
                                              unsigned long           key );

void         DIRECT_API *direct_hash_lookup ( const DirectHash       *hash,
                                              unsigned long           key );

void         DIRECT_API  direct_hash_iterate( DirectHash             *hash,
                                              DirectHashIteratorFunc  func,
                                              void                   *ctx );

#endif

