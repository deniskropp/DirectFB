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

#ifndef __DIRECT__HASH_H__
#define __DIRECT__HASH_H__

#include <direct/types.h>


typedef bool (*DirectHashIteratorFunc)( DirectHash *hash,
                                        __u32       key,
                                        void       *value,
                                        void       *ctx );


DirectResult  direct_hash_create ( int          size,
                                   DirectHash **ret_hash );

void          direct_hash_destroy( DirectHash  *hash );

DirectResult  direct_hash_insert ( DirectHash  *hash,
                                   __u32        key,
                                   void        *value );

void          direct_hash_remove ( DirectHash  *hash,
                                   __u32        key );

void         *direct_hash_lookup ( DirectHash  *hash,
                                   __u32        key );

void          direct_hash_iterate( DirectHash             *hash,
                                   DirectHashIteratorFunc  func,
                                   void                   *ctx );

#endif

