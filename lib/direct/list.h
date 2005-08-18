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
     DirectLink *prev; /* The 'prev' pointer of the first element always points
                          to the last element of the list, for fast appending ;-) */
};

static inline void
direct_list_prepend( DirectLink **list, DirectLink *link )
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

static inline void
direct_list_append( DirectLink **list, DirectLink *link )
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

static inline bool
direct_list_contains_element_EXPENSIVE( DirectLink *list, DirectLink *link )
{
     D_MAGIC_ASSERT_IF( list, DirectLink );
     
     if (!link->prev && !link->next)
          return false;
     
//     D_MAGIC_ASSERT( link, DirectLink );

     while (list) {
          if (list == link)
               return true;

          list = list->next;
     }

     return false;
}

static inline int
direct_list_count_elements_EXPENSIVE( DirectLink *list )
{
     int count = 0;

     while (list) {
          D_MAGIC_ASSERT( list, DirectLink );

          count++;
          
          list = list->next;
     }

     return count;
}

static inline void
direct_list_remove( DirectLink **list, DirectLink *link )
{
     DirectLink *next;
     DirectLink *prev;

     D_ASSERT( direct_list_contains_element_EXPENSIVE( *list, link ) );

     D_MAGIC_ASSERT( *list, DirectLink );
     D_MAGIC_ASSERT( link, DirectLink );

     next = link->next;
     prev = link->prev;

     if (next) {
          D_MAGIC_ASSERT( next, DirectLink );

          next->prev = prev;
     }
     else
         (*list)->prev = prev;

     if (link == *list)
          *list = next;
     else {
          D_MAGIC_ASSERT( prev, DirectLink );

          prev->next = next;
     }

     link->next = link->prev = NULL;

     D_MAGIC_CLEAR( link );
}

static inline bool
direct_list_check_link( const DirectLink *link )
{
     D_MAGIC_ASSERT_IF( link, DirectLink );

     return link != NULL;
}


#define direct_list_foreach(elem, list)                     \
     for (elem = (typeof(elem))(list);                      \
          direct_list_check_link( (DirectLink*)(elem) );    \
          elem = (typeof(elem))(((DirectLink*)(elem))->next))

#define direct_list_foreach_safe(elem, temp, list)                                             \
     for (elem = (typeof(elem))(list), temp = ((typeof(temp))(elem) ? (typeof(temp))(((DirectLink*)(elem))->next) : NULL); \
          direct_list_check_link( (DirectLink*)(elem) );                                       \
          elem = (typeof(elem))(temp), temp = ((typeof(temp))(elem) ? (typeof(temp))(((DirectLink*)(elem))->next) : NULL))

#endif

