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

#ifndef __DIRECT__LIST_H__
#define __DIRECT__LIST_H__

#include <direct/types.h>
#include <direct/debug.h>

struct __D_DirectLink {
     int         magic;

     DirectLink *next;
     DirectLink *prev; /* 'prev' of the first element points to the last element of the list ;) */
};

static inline void direct_list_prepend( DirectLink **list, DirectLink *link )
{
     DirectLink *first = *list;

     link->next = first;

     if (first) {
          D_MAGIC_ASSERT( first, DirectLink );

          link->prev = first->prev;

          first->prev = link;
     }
     else
          link->prev = link;

     *list = link;

     D_MAGIC_SET( link, DirectLink );
}

static inline void direct_list_append( DirectLink **list, DirectLink *link )
{
     DirectLink *first = *list;

     link->next = NULL;

     if (first) {
          DirectLink *last = first->prev;

          D_MAGIC_ASSERT( first, DirectLink );
          D_MAGIC_ASSERT( last, DirectLink );

          link->prev = last;

          last->next = first->prev = link;
     }
     else
          *list = link->prev = link;

     D_MAGIC_SET( link, DirectLink );
}

static inline void direct_list_remove( DirectLink **list, DirectLink *link )
{
     DirectLink *next = link->next;
     DirectLink *prev = link->prev;

     D_MAGIC_ASSERT( link, DirectLink );

     if (next) {
          D_MAGIC_ASSERT( next, DirectLink );

          next->prev = prev;
     }

     if (link == *list)
          *list = next;
     else {
          D_MAGIC_ASSERT( prev, DirectLink );

          prev->next = next;
     }

     link->next = link->prev = NULL;

     D_MAGIC_CLEAR( link );
}

static inline bool direct_list_check_link( const DirectLink *link )
{
     D_ASSERT( link == NULL || link->magic == D_MAGIC( "DirectLink" ) );

     return link != NULL;
}


#define direct_list_foreach(elem, list)                     \
     for (elem = (void*)(list);                             \
          direct_list_check_link( (DirectLink*)(elem) );    \
          elem = (void*)(((DirectLink*)(elem))->next))

#define direct_list_foreach_safe(elem, temp, list)                                             \
     for (elem = (void*)(list), temp = ((elem) ? (void*)(((DirectLink*)(elem))->next) : NULL); \
          direct_list_check_link( (DirectLink*)(elem) );                                       \
          elem = (void*)(temp), temp = ((elem) ? (void*)(((DirectLink*)(elem))->next) : NULL))

#endif

