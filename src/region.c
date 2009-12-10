/*
   (c) Copyright 2009  directfb.org

   All rights reserved.

   Written by Niels Roest <niels@directfb.org>.

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

#include "region.h"

void dfb_linkregionpool_init( DFBLinkRegionPool *pool, DFBLinkRegion *regions, int number )
{
     pool->regions = regions;
     pool->malloc  = 0;
     pool->number  = number;
     pool->free    = 0;
}

void dfb_linkregionpool_delete( DFBLinkRegionPool *pool )
{
     if (pool->malloc)
          free(pool->malloc);
}

DFBLinkRegion *dfb_linkregionpool_get( DFBLinkRegionPool *pool, DFBRegion *r )
{
     DFBLinkRegion *lr;

     /* check OOM */
     if (pool->free == pool->number) {
          if (pool->malloc) {
               D_WARN("Out of link regions!");
               return NULL;
          }
          else {
               const int extra = 400; /* pray if this is not enough */
               pool->free   = 0;
               pool->number = extra;
               pool->malloc = malloc( sizeof(DFBLinkRegion) * extra );
               if (!pool->malloc) {
                    D_WARN("out of memory!");
                    return NULL;
               }
          }
     }

     if (pool->malloc)
          lr = pool->malloc + pool->free;
     else
          lr = pool->regions + pool->free;

     pool->free++;

     if (r) 
          lr->region = *r;

     lr->link.magic = 0;
     return lr;
}

/* Add only the allowed part of region "region" to the region list "list". 
 * This creates at most 4 new regions, on all sides of "forbidden".
 */
void dfb_linkregionpool_add_allowedpartofregion( DFBLinkRegionPool *pool,
                                                 DirectLink **list,
                                                 DFBRegion *region,
                                                 DFBRegion *forbidden )
{
     DFBRegion *r = forbidden;

     if (region->y1 < r->y1) { /* up */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.y2 = r->y1-1;
          direct_list_append( list, &lr->link );
     }
     if (region->y2 > r->y2) { /* down */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.y1 = r->y2+1;
          direct_list_append( list, &lr->link );
     }
     if (region->x1 < r->x1) { /* left */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.x2 = r->x1-1;
          if (r->y1 > region->y1)
               lr->region.y1 = r->y1;
          if (r->y2 < region->y2)
               lr->region.y2 = r->y2;
          direct_list_append( list, &lr->link );
     }
     if (region->x2 > r->x2) { /* right */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.x1 = r->x2+1;
          if (r->y1 > region->y1)
               lr->region.y1 = r->y1;
          if (r->y2 < region->y2)
               lr->region.y2 = r->y2;
          direct_list_append( list, &lr->link );
     }
}

/* find combinable regions.
 * We assume all rectangles are non-overlapping.
 * We look for:
 *   (1) rectangle to the right, connecting in height
 *   (2) rectangle to the bottom, connecting in width
 * top/left is caught too by this principle
 * 
 * We sort the list in-place: entries will be removed only.
 * We never remove the first entry.
 */
void dfb_collate( DirectLink **updates )
{
     DFBLinkRegion *linkRegion, *lr;

     collate_restart:

     direct_list_foreach(linkRegion, *updates) {
          /* compare against item+1..last */
          lr = (DFBLinkRegion*)(linkRegion->link.next);
          while (lr) {
               /* to the right */
               if (    (linkRegion->region.y1 == lr->region.y1)
                    && (linkRegion->region.y2 == lr->region.y2)
                    && (linkRegion->region.x2 == lr->region.x1 - 1) ) {
                    /* fold */
                    linkRegion->region.x2 = lr->region.x2;
                    direct_list_remove( updates, &lr->link );
                    goto collate_restart;
               }
               /* to the bottom */
               if (    (linkRegion->region.x1 == lr->region.x1)
                    && (linkRegion->region.x2 == lr->region.x2)
                    && (linkRegion->region.y2 == lr->region.y1 - 1) ) {
                    /* fold */
                    linkRegion->region.y2 = lr->region.y2;
                    direct_list_remove( updates, &lr->link );
                    goto collate_restart;
               }
               lr = (DFBLinkRegion*)(lr->link.next);
          }
     }
}
