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

#include <direct/list.h>
#include <directfb_util.h>

typedef struct {
     DirectLink link;
     DFBRegion  region;
} DFBLinkRegion;

typedef struct {
     DFBLinkRegion *regions;
     DFBLinkRegion *malloc;
     int number;
     int free;
} DFBLinkRegionPool;

/* a linkregionpool is a pool of LinkRegions, as simple as that.
   You can get empty (e.g. zeroed content) LinkRegions from the pool.
*/

/* initialize the regionpool with some initial amount of (empty) regions */
void
dfb_linkregionpool_init( DFBLinkRegionPool *pool,
                         DFBLinkRegion *regions,
                         int number );

/* delete the regionpool.
   This will also free any memory that was allocated due to _get() */
void
dfb_linkregionpool_delete( DFBLinkRegionPool *pool );

/* get an empty region, this will return a usable LinkRegion.
   You can provide a DFBRegion to init the region part of the LinkRegion, or pass NULL.
   If the pool is out of memory, a malloc() or malloc()/copy will be done. 
   On OOM, NULL will be returned. */
DFBLinkRegion *
dfb_linkregionpool_get( DFBLinkRegionPool *pool,
                        DFBRegion *r );

/* Add only the allowed part of region "region" to the region list "list". 
   This creates at most 4 new regions, on all sides of "forbidden".
*/
void
dfb_linkregionpool_add_allowedpartofregion( DFBLinkRegionPool *pool,
                                            DirectLink **list,
                                            DFBRegion *region,
                                            DFBRegion *forbidden );

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
void
dfb_collate( DirectLink **updates );
