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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <errno.h>

#include <directfb.h>

#include <direct/types.h>
#include <direct/debug.h>

/*
 * translates errno to DirectFB DFBResult
 */
DFBResult errno2dfb( int erno );

bool dfb_region_intersect( DFBRegion *region,
                           int x1, int y1, int x2, int y2 );

bool dfb_region_region_intersect( DFBRegion       *region,
                                  const DFBRegion *clip );

bool dfb_region_rectangle_intersect( DFBRegion          *region,
                                     const DFBRectangle *rect );

bool dfb_unsafe_region_intersect( DFBRegion *region,
                                  int x1, int y1, int x2, int y2 );

bool dfb_unsafe_region_rectangle_intersect( DFBRegion          *region,
                                            const DFBRectangle *rect );

bool dfb_rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                               DFBRegion    *region );

bool dfb_rectangle_intersect( DFBRectangle       *rectangle,
                              const DFBRectangle *clip );

/* returns the result in the first rectangle */
void dfb_rectangle_union ( DFBRectangle       *rect1,
                           const DFBRectangle *rect2 );

static inline void dfb_rectangle_from_region( DFBRectangle    *rect,
                                              const DFBRegion *region )
{
     D_ASSERT( rect != NULL );
     D_ASSERT( region != NULL );

     rect->x = region->x1;
     rect->y = region->y1;
     rect->w = region->x2 - region->x1 + 1;
     rect->h = region->y2 - region->y1 + 1;
}

static inline void dfb_region_from_rectangle( DFBRegion          *region,
                                              const DFBRectangle *rect )
{
     D_ASSERT( region != NULL );
     D_ASSERT( rect != NULL );

     region->x1 = rect->x;
     region->y1 = rect->y;
     region->x2 = rect->x + rect->w - 1;
     region->y2 = rect->y + rect->h - 1;
}

/* Returns the current time after startup of DirectFB in microseconds */
long long dfb_get_micros();

/* Returns the current time after startup of DirectFB in milliseconds */
long long dfb_get_millis();


#endif
