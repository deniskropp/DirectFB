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

struct __D_DirectLink {
     DirectLink *next;
     DirectLink *prev; /* 'prev' of the first element points to the last element of the list ;) */
};

static inline void direct_list_prepend (DirectLink **list, DirectLink *link)
{
     DirectLink *first = *list;

     link->next = first;

     if (first) {
          link->prev = first->prev;

          first->prev = link;
     }
     else
          link->prev = link;

     *list = link;
}

static inline void direct_list_append (DirectLink **list, DirectLink *link)
{
     DirectLink *first = *list;

     link->next = NULL;

     if (first) {
          DirectLink *last = first->prev;

          link->prev = last;

          last->next = first->prev = link;
     }
     else
          *list = link->prev = link;
}

static inline void direct_list_remove (DirectLink **list, DirectLink *link)
{
     DirectLink *next = link->next;
     DirectLink *prev = link->prev;

     if (next)
          next->prev = prev;

     if (link == *list)
          *list = next;
     else
          prev->next = next;

     link->next = link->prev = NULL;
}


#define direct_list_foreach(link, list) \
     for (link = list; link; link = link->next)

#define direct_list_foreach_safe(link, temp, list)         \
     for (link = list, temp = (link ? link->next : NULL);  \
          link;                                            \
          link = temp, temp = (link ? link->next : NULL))

#endif

